#include "ff_player.h"

#define BUFFER_SIZE 4096
#define MAX_CHANNELS 6


static void * audio_thread_func(void * arg);
static void * video_thread_func(void * arg);
static bool ffmpeg_pix_fmt_has_alpha(enum AVPixelFormat pix_fmt);
static int ffmpeg_image_allocate(ff_player_t * player);
static uint8_t * ffmpeg_get_img_data(ff_player_t * player);

ff_player_t * player_create()
{
    ff_player_t * player = malloc(sizeof(ff_player_t));
    if(!player) return NULL;

    memset(player, 0, sizeof(ff_player_t));

    // 初始化互斥锁
    pthread_mutex_init(&player->mutex, NULL);

    // 初始化状态
    player->state = PLAYER_STOPPED;
    player->seek_request = 0;
    player->current_pts = 0;

    return player;
}

int player_open(ff_player_t * player, const char * filename)
{
    if(!player) return -1;

    pthread_mutex_lock(&player->mutex);

    // 如果已经在播放，直接返回
    if(player->state == PLAYER_PLAYING) {
        pthread_mutex_unlock(&player->mutex);
        return -2;
    }

    player->filename = strdup(filename);

    int ret = 0;

    // 打开音频文件
    if(avformat_open_input(&player->format_ctx, player->filename, NULL, NULL) < 0) {
        fprintf(stderr, "无法打开文件\n");
        ret = -1;
        goto cleanup;
    }

    if(avformat_find_stream_info(player->format_ctx, NULL) < 0) {
        fprintf(stderr, "无法获取流信息\n");
        ret = -1;
        goto cleanup;
    }

    pthread_mutex_unlock(&player->mutex);
    return 0;

cleanup:
    player_stop(player);
    pthread_mutex_unlock(&player->mutex);
    return ret;
}

int player_init_audio(ff_player_t * player)
{
    if(!player) return -1;
    pthread_mutex_lock(&player->mutex);

    int ret = 0;

    // 查找音频流
    player->audio_stream_index = -1;
    for(int i = 0; i < player->format_ctx->nb_streams; i++) {
        if(player->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            player->audio_stream_index = i;
            break;
        }
    }

    if(player->audio_stream_index == -1) {
        fprintf(stderr, "未找到音频流\n");
        ret = -1;
        goto cleanup;
    }

    // 获取解码器
    AVCodecParameters * codecpar = player->format_ctx->streams[player->audio_stream_index]->codecpar;
    const AVCodec * codec        = avcodec_find_decoder(codecpar->codec_id);
    if(!codec) {
        fprintf(stderr, "未找到对应的解码器\n");
        ret = -1;
        goto cleanup;
    }

    player->audio_codec_ctx = avcodec_alloc_context3(codec);
    if(!player->audio_codec_ctx) {
        fprintf(stderr, "无法分配解码器上下文\n");
        ret = -1;
        goto cleanup;
    }

    if(avcodec_parameters_to_context(player->audio_codec_ctx, codecpar) < 0) {
        fprintf(stderr, "无法复制编解码器参数到解码器上下文\n");
        ret = -1;
        goto cleanup;
    }

    if(avcodec_open2(player->audio_codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "无法打开解码器\n");
        ret = -1;
        goto cleanup;
    }

    // 设置重采样
    player->swr_ctx = swr_alloc();
    if(!player->swr_ctx) {
        fprintf(stderr, "无法分配重采样上下文\n");
        ret = -1;
        goto cleanup;
    }

    // 设置重采样参数
    av_opt_set_int(player->swr_ctx, "in_channel_layout", av_get_default_channel_layout(player->audio_codec_ctx->channels), 0);
    av_opt_set_int(player->swr_ctx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(player->swr_ctx, "in_sample_rate", player->audio_codec_ctx->sample_rate, 0);
    av_opt_set_int(player->swr_ctx, "out_sample_rate", 44100, 0);
    av_opt_set_sample_fmt(player->swr_ctx, "in_sample_fmt", player->audio_codec_ctx->sample_fmt, 0);
    av_opt_set_sample_fmt(player->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if(swr_init(player->swr_ctx) < 0) {
        fprintf(stderr, "无法初始化重采样器\n");
        ret = -1;
        goto cleanup;
    }

    player->sample_rate = 44100;
    player->channels    = 2;

    // 打开ALSA设备
    int err;
    if((err = snd_pcm_open(&player->pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "无法打开PCM设备: %s\n", snd_strerror(err));
        ret = -1;
        goto cleanup;
    }

    // 配置PCM参数
    snd_pcm_hw_params_t * hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    snd_pcm_hw_params_any(player->pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(player->pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(player->pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(player->pcm_handle, hw_params, player->channels);
    snd_pcm_hw_params_set_rate_near(player->pcm_handle, hw_params, &player->sample_rate, 0);

    player->frames = 1024;
    snd_pcm_hw_params_set_period_size_near(player->pcm_handle, hw_params, &player->frames, 0);

    if((err = snd_pcm_hw_params(player->pcm_handle, hw_params)) < 0) {
        fprintf(stderr, "无法设置硬件参数: %s\n", snd_strerror(err));
        ret = -1;
        goto cleanup;
    }

    player_init_volume_control(player);

    // 获取持续时间
    AVStream * audio_stream = player->format_ctx->streams[player->audio_stream_index];
    player->time_base       = audio_stream->time_base;
    player->duration        = audio_stream->duration; // 使用流的duration而不是format_ctx的

    // 创建播放线程
    player->state = PLAYER_PAUSED;
    if(pthread_create(&player->audio_thread, NULL, audio_thread_func, player) != 0) {
        fprintf(stderr, "无法创建播放线程\n");
        player->state = PLAYER_STOPPED;
        ret = -1;
        goto cleanup;
    }

    pthread_mutex_unlock(&player->mutex);
    return 0;

cleanup:
    player_stop(player);
    pthread_mutex_unlock(&player->mutex);
    return ret;
}

int player_init_video(ff_player_t * player, lv_obj_t * lv_obj)
{
    if(!player || !lv_obj) return -1;

    pthread_mutex_lock(&player->mutex);

    int ret            = 0;
    player->video_area = lv_obj;

    // 查找视频流
    player->video_stream_index = -1;
    for(int i = 0; i < player->format_ctx->nb_streams; i++) {
        if(player->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            player->video_stream_index = i;
            break;
        }
    }

    if(player->video_stream_index == -1) {
        fprintf(stderr, "未找到视频流\n");
        ret = -1;
        goto cleanup;
    }

    // 获取解码器
    AVCodecParameters * codecpar = player->format_ctx->streams[player->video_stream_index]->codecpar;
    const AVCodec * codec        = avcodec_find_decoder(codecpar->codec_id);
    if(!codec) {
        fprintf(stderr, "未找到对应的解码器\n");
        ret = -1;
        goto cleanup;
    }

    player->video_codec_ctx = avcodec_alloc_context3(codec);
    if(!player->video_codec_ctx) {
        fprintf(stderr, "无法分配解码器上下文\n");
        ret = -1;
        goto cleanup;
    }

    if(avcodec_parameters_to_context(player->video_codec_ctx, codecpar) < 0) {
        fprintf(stderr, "无法复制编解码器参数到解码器上下文\n");
        ret = -1;
        goto cleanup;
    }

    if(avcodec_open2(player->video_codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "无法打开解码器\n");
        ret = -1;
        goto cleanup;
    }

    if(ffmpeg_image_allocate(player) < 0) {
        LV_LOG_ERROR("ffmpeg image allocate failed");
        ret = -1;
        goto cleanup;
    }

    // 在img_dsc对象里写入图像参数
    bool has_alpha = ffmpeg_pix_fmt_has_alpha(player->video_codec_ctx->pix_fmt);
    int width           = player->video_codec_ctx->width;
    int height           = player->video_codec_ctx->height;
    player->video_dst_pix_fmt = (has_alpha ? AV_PIX_FMT_BGRA : AV_PIX_FMT_BGR0);

    uint32_t data_size      = 0;
    if(has_alpha)    data_size = width * height * LV_IMG_PX_SIZE_ALPHA_BYTE;
    else             data_size = width * height * LV_COLOR_SIZE / 8;

    player->img_dsc.header.always_zero = 0;
    player->img_dsc.header.w           = width;
    player->img_dsc.header.h           = height;
    player->img_dsc.data_size          = data_size;
    player->img_dsc.header.cf          = has_alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
    player->img_dsc.data               = player->video_dst_data[0];

    printf("width=%d, height=%d, data_size=%d\n", width, height, data_size);

    //运行到这一句，程序崩溃
    lv_img_set_src(player->video_area, &(player->img_dsc));

    ret = 0;

    /*
    if(pthread_create(&player->video_thread, NULL, video_thread_func, player) != 0) {
        fprintf(stderr, "无法创建视频线程\n");
        goto cleanup;
    }

    pthread_mutex_unlock(&player->mutex);
    */
    return ret;

cleanup:
    avcodec_free_context(&player->video_codec_ctx);
    return ret;
}

/**
 * 分配对象
 */
static int ffmpeg_image_allocate(ff_player_t * player)
{
    int ret;

    /* allocate image where the decoded image will be put */
    ret = av_image_alloc(player->video_src_data, player->video_src_linesize, player->video_codec_ctx->width,
                         player->video_codec_ctx->height, player->video_codec_ctx->pix_fmt, 4);

    if(ret < 0) {
        LV_LOG_ERROR("Could not allocate src raw video buffer");
        return ret;
    }

    LV_LOG_INFO("alloc video_src_bufsize = %d", ret);

    ret = av_image_alloc(player->video_dst_data, player->video_dst_linesize, player->video_codec_ctx->width,
                         player->video_codec_ctx->height, player->video_dst_pix_fmt, 4);

    if(ret < 0) {
        LV_LOG_ERROR("Could not allocate dst raw video buffer");
        return ret;
    }

    LV_LOG_INFO("allocate video_dst_bufsize = %d", ret);

    player->frame = av_frame_alloc();

    if(player->frame == NULL) {
        LV_LOG_ERROR("Could not allocate frame");
        return -1;
    }

    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&player->pkt);
    player->pkt.data = NULL;
    player->pkt.size = 0;

    return 0;
}

static bool ffmpeg_pix_fmt_has_alpha(enum AVPixelFormat pix_fmt)
{
    const AVPixFmtDescriptor * desc = av_pix_fmt_desc_get(pix_fmt);

    if(desc == NULL) {
        return false;
    }

    if(pix_fmt == AV_PIX_FMT_PAL8) {
        return true;
    }

    return (desc->flags & AV_PIX_FMT_FLAG_ALPHA) ? true : false;
}

static void * audio_thread_func(void * arg)
{
    ff_player_t * player = (ff_player_t *)arg;

    AVPacket * packet = av_packet_alloc();
    AVFrame * frame   = av_frame_alloc();
    if(!packet || !frame) {
        fprintf(stderr, "无法分配数据包或帧\n");
        goto cleanup;
    }

    uint8_t * audio_buffer = malloc(BUFFER_SIZE * player->channels * 2); // S16LE
    if(!audio_buffer) {
        fprintf(stderr, "无法分配音频缓冲区\n");
        goto cleanup;
    }

    while(player->state != PLAYER_STOPPED) {

        // 检查跳转请求
        if(player->seek_request) {
            int64_t seek_target = player->seek_pos;
            if(av_seek_frame(player->format_ctx, player->audio_stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
                fprintf(stderr, "跳转失败\n");
            } else {
                avcodec_flush_buffers(player->audio_codec_ctx);
                player->current_pts = seek_target;
            }
            player->seek_request = 0;
        }
        // 检查暂停状态
        if(player->state == PLAYER_PAUSED) {
            usleep(100000); // 100ms
            continue;
        }

        int ret = av_read_frame(player->format_ctx, packet);
        if(ret < 0) {
            // 文件结束或错误
            //break;
            player->state = PLAYER_PAUSED;
            continue;
        }

        if(packet->stream_index == player->audio_stream_index) {
            ret = avcodec_send_packet(player->audio_codec_ctx, packet);
            if(ret < 0) {
                av_packet_unref(packet);
                continue;
            }

            while(ret >= 0) {
                ret = avcodec_receive_frame(player->audio_codec_ctx, frame);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if(ret < 0) {
                    fprintf(stderr, "解码错误\n");
                    break;
                }

                // 更新当前播放位置
                player->current_pts = frame->pts;

                // 重采样
                uint8_t * out_data[1] = {audio_buffer};
                int out_samples = swr_convert(player->swr_ctx, out_data, BUFFER_SIZE, (const uint8_t **)frame->data,
                                              frame->nb_samples);

                if(out_samples > 0) {
                    int data_size = out_samples * player->channels * 2; // S16LE

                    // 写入ALSA设备
                    snd_pcm_sframes_t frames_written = snd_pcm_writei(player->pcm_handle, audio_buffer, out_samples);
                    if(frames_written < 0) {
                        frames_written = snd_pcm_recover(player->pcm_handle, frames_written, 0);
                    }
                    if(frames_written < 0) {
                        fprintf(stderr, "写入PCM设备错误: %s\n", snd_strerror(frames_written));
                    }
                }

                av_frame_unref(frame);
            }
        }

        av_packet_unref(packet);
    }

cleanup:
    if(packet) av_packet_free(&packet);
    if(frame) av_frame_free(&frame);
    if(audio_buffer) free(audio_buffer);

    return NULL;
}

static void * video_thread_func(void * arg)
{
    ff_player_t * player = (ff_player_t *)arg;

    while(player->state != PLAYER_STOPPED) {

    }
}

int player_pause(ff_player_t * player)
{
    if(!player) return -1;

    if(player->state == PLAYER_PLAYING) {
        player->state = PLAYER_PAUSED;
        snd_pcm_pause(player->pcm_handle, 1);
        return 0;
    }
    return -1;
}

int player_resume(ff_player_t * player)
{
    if(!player) return -1;

    if(player->state == PLAYER_PAUSED) {
        player->state = PLAYER_PLAYING;
        snd_pcm_pause(player->pcm_handle, 0);
        return 0;
    }
    return -1;
}

int player_stop(ff_player_t * player)
{
    if(!player) return -1;

    pthread_mutex_lock(&player->mutex);

    player->state = PLAYER_STOPPED;

    pthread_mutex_unlock(&player->mutex);

    // 等待线程结束
    if(player->audio_thread) {
        pthread_join(player->audio_thread, NULL);
        player->audio_thread = 0;
    }

    if(player->video_thread) {
        pthread_join(player->video_thread, NULL);
        player->video_thread = 0;
    }

    pthread_mutex_lock(&player->mutex);

    // 清理资源
    if(player->mixer) {
        snd_mixer_close(player->mixer);
        player->mixer = NULL;
        player->elem  = NULL;
    }

    if(player->pcm_handle) {
        snd_pcm_drain(player->pcm_handle);
        snd_pcm_close(player->pcm_handle);
        player->pcm_handle = NULL;
    }

    if(player->swr_ctx) {
        swr_free(&player->swr_ctx);
        player->swr_ctx = NULL;
    }

    if(player->audio_codec_ctx) {
        avcodec_free_context(&player->audio_codec_ctx);
        player->audio_codec_ctx = NULL;
    }

    if(player->format_ctx) {
        avformat_close_input(&player->format_ctx);
        player->format_ctx = NULL;
    }

    pthread_mutex_unlock(&player->mutex);
    return 0;
}

//根据百分比跳转
int player_seek_pct(ff_player_t * player, double percent)
{
    int64_t target_pts = (int64_t)(player->duration * percent / 100.0);
    int64_t now_pts    = player->current_pts;

    printf("now=%lld, duration=%lld\n", now_pts, player->duration);

    if(!player || player->state == PLAYER_STOPPED) return -1;
    if(target_pts < 0) target_pts = 0;
    if(target_pts > player->duration) target_pts = player->duration;

    player->seek_pos = target_pts;
    player->seek_request = 1;
    return 0;

}

//根据毫秒数跳转
int player_seek_ms(ff_player_t * player, int64_t target_ms)
{
    if(player->state != PLAYER_STOPPED) {
        int64_t target_pts = target_ms * (AV_TIME_BASE / 1000);
        int64_t now_pts    = player->current_pts;

        printf("now=%lld, duration=%lld\n", now_pts, player->duration);
        if(!player || target_pts < 0 || target_pts > player->duration || player->state == PLAYER_STOPPED)
            return -1;
        player->seek_pos = target_pts;
        player->seek_request = 1;
        return 0;
    }
    return -1;
}

int64_t player_get_position_ms(ff_player_t * player)
{
    if(!player || player->duration <= 0) return 0;

    int64_t current = player->current_pts;
    return current / (AV_TIME_BASE / 1000);
}

int64_t player_get_duration_ms(ff_player_t * player)
{
    if(!player || player->duration <= 0) return 0;
    return player->duration / (AV_TIME_BASE / 1000);
}

double player_get_position_pct(ff_player_t * player)
{
    if(!player || player->duration <= 0) return 0.0;
    int64_t current = player->current_pts;
    return (double)current / player->duration * 100.0;
}

player_state_t player_get_state(ff_player_t * player)
{
    if(!player) return PLAYER_STOPPED;
    return player->state;
}

void player_destroy(ff_player_t * player)
{
    if(!player) return;

    player_stop(player);

    if(player->filename) {
        free(player->filename);
        player->filename = NULL;
    }

    pthread_mutex_destroy(&player->mutex);
    free(player);
}

int player_init_volume_control(ff_player_t * player)
{
    int ret;
    const char * card       = "default";
    const char * selem_name = "LINEOUT volume";

    if((ret = snd_mixer_open(&player->mixer, 0)) < 0) {
        goto cleanup;
    }

    if((ret = snd_mixer_attach(player->mixer, card)) < 0) {
        fprintf(stderr, "无法附加mixer到卡: %s\n", snd_strerror(ret));
        goto cleanup;
    }

    if((ret = snd_mixer_selem_register(player->mixer, NULL, NULL)) < 0) {
        fprintf(stderr, "无法注册mixer: %s\n", snd_strerror(ret));
        goto cleanup;
    }

    if((ret = snd_mixer_load(player->mixer)) < 0) {
        fprintf(stderr, "无法加载mixer: %s\n", snd_strerror(ret));
        goto cleanup;
    }

    // 查找音量控制元素
    snd_mixer_selem_id_t * sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);

    player->elem = snd_mixer_find_selem(player->mixer, sid);
    if(!player->elem) {
        fprintf(stderr, "无法找到音量控制元素 '%s'\n", selem_name);
        goto cleanup;
    }


    // 获取音量范围
    snd_mixer_selem_get_playback_volume_range(player->elem, &player->volume_min, &player->volume_max);

    // 获取实际音量并计算百分比
    long actual_volume;
    snd_mixer_selem_get_playback_volume(player->elem, 0, &actual_volume);
    player->volume     = 100 * (actual_volume - player->volume_min) / (player->volume_max - player->volume_min);

    printf("音量: %ld (%ld, %ld-%ld)\n", player->volume, actual_volume, player->volume_min, player->volume_max);
    return 0;

cleanup:
    snd_mixer_close(player->mixer);
    player->mixer = NULL;
    return -1;
}

int player_set_volume(ff_player_t * player, int volume)
{
    if(!player || !player->elem) return -1;

    if(volume < 0) volume = 0;
    if(volume > 100) volume = 100;

    player->volume = volume;

    // 将百分比转换为实际音量值
    long actual_volume = player->volume_min + (volume * (player->volume_max - player->volume_min)) / 100;

    int ret = snd_mixer_selem_set_playback_volume_all(player->elem, actual_volume);
    if(ret < 0) {
        fprintf(stderr, "设置音量失败: %s\n", snd_strerror(ret));
        return -1;
    }

    return 0;
}

int player_get_volume(ff_player_t * player)
{
    if(!player) return -1;
    return player->volume;
}