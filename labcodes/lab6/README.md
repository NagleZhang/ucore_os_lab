# lab6 遇到的一些问题

proc do_exit failed.

这个时候, 自我怀疑非常之严重, 仅仅是一点小失误, 就会觉得自己是不是很多事情都没有做好. 这种心态不太好.

实际上,就是一个用户态以及内核态切换的时候, 执行出了一些问题. do exit 了.

仔细看了一下报错, 是 unhandled trap. 所以是在 trap frame 的时候, 抛出了异常, 具体的 trap number 是 13, General Protection.

猜测是越级导致的错误. 也就是说, 用户态访问了内核态. 进而做了 diff, 发现实际上是 lab5 的代码没有被 copy 过来导致的.

修复后, 一切正常
