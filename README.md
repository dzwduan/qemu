# qemu nutshell support

# build qemu

refer build.sh

# run qemu 

refer run.sh

# 修改过程

## add nutshell board
在加入uart之前，kconfig中select SERIAL 必须加，否则报错

## add uart pflash plic



# 参考资料
1. https://airbus-seclab.github.io/qemu_blog/
1. https://fgoehler.com/blog/adding-a-new-architecture-to-qemu-01/
2. https://quard-star-tutorial.readthedocs.io/zh-cn/latest