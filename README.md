# !此项目为2次制作!
* 原项目地址
https://github.com/RobinNotBad/lvgl_v833_q3

## 项目部分是用AI写的
**代码写的很神奇**
~~能跑起来就行嘻嘻~~ 

## 运行平台

目前支持：**阿尔法蛋词典笔Q3（普通版、增强版）**

SOC：**全志V833**

可以实现简易的文件浏览、MP3、小游戏等功能

## 一些截图 <br>
<img width="240" height="240" alt="screen_20260305_204625" src="https://github.com/user-attachments/assets/39bdb3d3-135b-48b2-9b01-f2078dcbd333" />
<img width="240" height="240" alt="screen_20260305_204442" src="https://github.com/user-attachments/assets/4e95439b-3b6e-4b4e-8d7a-a2ac0895bbea" />
<img width="240" height="240" alt="screen_20260305_204629" src="https://github.com/user-attachments/assets/384b2831-f009-482c-9400-797526cc62f0" />

## 怎么从项目源代码编译
* 建议使用**linux系统** 
* 克隆项目到一个空文件夹中  
* 安装交叉编译器
> 例如armhf musl gcc编译器
* 将项目文件中` build.sh `的文件路径改为你的编译器路径<br>
* 运行 ` ./build.sh `
* 目标文件为` demo `

## 其他
* 字体使用了：阿里巴巴普惠体 Medium、FontAwesome 5 Free Solid，为了在lvgl中正常使用图标，对这两个字体进行了合并。若要换用自己的字体，可以使用FontForge软件，将FontAwesome中 #61440 之后的所有图标复制到现有字体中，再进行大小缩放。
