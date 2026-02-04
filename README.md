# !此项目为2次制作!
* 原项目地址
https://github.com/RobinNotBad/lvgl_v833_q3

## 项目部分是用AI写的
**代码写的很神奇**
~~能跑起来就行嘻嘻~~ <br>

## 运行平台
**soc 志强v833 armv7 32bit**  
推荐在**阿尔法蛋q3词典笔**上运行  

可以实现简易的文件浏览、MP3、小游戏等功能

## 怎么从项目源代码编译
* 建议使用**linux系统** 
* 克隆项目到一个空文件夹中  
* 安装交叉编译器
> 例如armhf musl gcc编译器
* 将项目文件中build.sh的文件路径改为你的编译器路径<br>
* 运行./build.sh
* 目标文件为"demo"

## 其他
* 字体使用了：**阿里巴巴普惠体** Medium、FontAwesome 5 Free Solid，为了在lvgl中正常使用图标，对这两个字体进行了合并。若要换用自己的字体，可以使用FontForge软件，将FontAwesome中 #61440 之后的所有图标复制到现有字体中，再进行大小缩放。
