# qemu nutshell support

# build qemu

refer build.sh

# run qemu 

refer run.sh

# 修改过程

## add nutshell board
在加入uart之前，kconfig中select SERIAL 必须加，否则报错

## add uart pflash plic
大部分偷的virt + shakti_c, 当前build能过，还没测, run直接报错，摆了明天修,
使用 https://toolchains.bootlin.com/ 进行测试, 测试代码在lowerboot dir.



# 参考资料
1. https://airbus-seclab.github.io/qemu_blog/
1. https://fgoehler.com/blog/adding-a-new-architecture-to-qemu-01/
2. https://quard-star-tutorial.readthedocs.io/zh-cn/latest