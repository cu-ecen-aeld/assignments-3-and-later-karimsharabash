#OOPS

```
[  111.010826] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
[  111.015396] Mem abort info:
[  111.015881]   ESR = 0x0000000096000045
[  111.016901]   EC = 0x25: DABT (current EL), IL = 32 bits
[  111.017365]   SET = 0, FnV = 0
[  111.017803]   EA = 0, S1PTW = 0
[  111.018628]   FSC = 0x05: level 1 translation fault
[  111.019002] Data abort info:
[  111.019271]   ISV = 0, ISS = 0x00000045
[  111.019547]   CM = 0, WnR = 1
[  111.020612] user pgtable: 4k pages, 39-bit VAs, pgdp=0000000043604000
[  111.021088] [0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
[  111.022231] Internal error: Oops: 0000000096000045 [#1] PREEMPT SMP
[  111.023061] Modules linked in: scull(O) faulty(O) hello(O)
[  111.024451] CPU: 0 PID: 444 Comm: sh Tainted: G           O      5.15.184-yocto-standard #1
[  111.024921] Hardware name: linux,dummy-virt (DT)
[  111.025585] pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[  111.025942] pc : faulty_write+0x18/0x20 [faulty]
[  111.026412] lr : vfs_write+0xf8/0x2a0
[  111.026930] sp : ffffffc0096cbd80
[  111.027189] x29: ffffffc0096cbd80 x28: ffffff800f9f4240 x27: 0000000000000000
[  111.027822] x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
[  111.028682] x23: 0000000000000000 x22: ffffffc0096cbdc0 x21: 000000557b255500
[  111.029351] x20: ffffff80036e8000 x19: 0000000000000012 x18: 0000000000000000
[  111.029755] x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
[  111.030440] x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
[  111.031014] x11: 0000000000000000 x10: 0000000000000000 x9 : ffffffc008271648
[  111.032451] x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
[  111.032847] x5 : 0000000000000001 x4 : ffffffc000b95000 x3 : ffffffc0096cbdc0
[  111.033548] x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
[  111.034496] Call trace:
[  111.034639]  faulty_write+0x18/0x20 [faulty]
[  111.035184]  ksys_write+0x74/0x110
[  111.035460]  __arm64_sys_write+0x24/0x30
[  111.035854]  invoke_syscall+0x5c/0x130
[  111.036174]  el0_svc_common.constprop.0+0x4c/0x100
[  111.037549]  do_el0_svc+0x4c/0xc0
[  111.038308]  el0_svc+0x28/0x80
[  111.038437]  el0t_64_sync_handler+0xa4/0x130
[  111.038724]  el0t_64_sync+0x1a0/0x1a4
[  111.039459] Code: d2800001 d2800000 d503233f d50323bf (b900003f)
[  111.040609] ---[ end trace 002fe6504f6eb732 ]---
Segmentation fault
Poky (Yocto Project Reference Distro) 4.0.27 qemuarm64 /dev/ttyAMA0
```
#Oops Analysis
1. This is a Null pointer Dereference caused a segmentation faullt
2. The fault happens in core 0 on PID 444 through sh shell
3. The function belongs to the "faulty" module and it called faulty_write
