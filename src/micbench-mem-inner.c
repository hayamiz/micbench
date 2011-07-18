__asm__ volatile(
"#seq inner loop\n"
"movq	0(%%rax), %%r8\n"
"addq	$1, 0(%%rax)\n"

"movq	8(%%rax), %%r8\n"
"addq	$1, 8(%%rax)\n"

"movq	16(%%rax), %%r8\n"
"addq	$1, 16(%%rax)\n"

"movq	24(%%rax), %%r8\n"
"addq	$1, 24(%%rax)\n"

"movq	32(%%rax), %%r8\n"
"addq	$1, 32(%%rax)\n"

"movq	40(%%rax), %%r8\n"
"addq	$1, 40(%%rax)\n"

"movq	48(%%rax), %%r8\n"
"addq	$1, 48(%%rax)\n"

"movq	56(%%rax), %%r8\n"
"addq	$1, 56(%%rax)\n"

"movq	64(%%rax), %%r8\n"
"addq	$1, 64(%%rax)\n"

"movq	72(%%rax), %%r8\n"
"addq	$1, 72(%%rax)\n"

"movq	80(%%rax), %%r8\n"
"addq	$1, 80(%%rax)\n"

"movq	88(%%rax), %%r8\n"
"addq	$1, 88(%%rax)\n"

"movq	96(%%rax), %%r8\n"
"addq	$1, 96(%%rax)\n"

"movq	104(%%rax), %%r8\n"
"addq	$1, 104(%%rax)\n"

"movq	112(%%rax), %%r8\n"
"addq	$1, 112(%%rax)\n"

"movq	120(%%rax), %%r8\n"
"addq	$1, 120(%%rax)\n"

"movq	128(%%rax), %%r8\n"
"addq	$1, 128(%%rax)\n"

"movq	136(%%rax), %%r8\n"
"addq	$1, 136(%%rax)\n"

"movq	144(%%rax), %%r8\n"
"addq	$1, 144(%%rax)\n"

"movq	152(%%rax), %%r8\n"
"addq	$1, 152(%%rax)\n"

"movq	160(%%rax), %%r8\n"
"addq	$1, 160(%%rax)\n"

"movq	168(%%rax), %%r8\n"
"addq	$1, 168(%%rax)\n"

"movq	176(%%rax), %%r8\n"
"addq	$1, 176(%%rax)\n"

"movq	184(%%rax), %%r8\n"
"addq	$1, 184(%%rax)\n"

"movq	192(%%rax), %%r8\n"
"addq	$1, 192(%%rax)\n"

"movq	200(%%rax), %%r8\n"
"addq	$1, 200(%%rax)\n"

"movq	208(%%rax), %%r8\n"
"addq	$1, 208(%%rax)\n"

"movq	216(%%rax), %%r8\n"
"addq	$1, 216(%%rax)\n"

"movq	224(%%rax), %%r8\n"
"addq	$1, 224(%%rax)\n"

"movq	232(%%rax), %%r8\n"
"addq	$1, 232(%%rax)\n"

"movq	240(%%rax), %%r8\n"
"addq	$1, 240(%%rax)\n"

"movq	248(%%rax), %%r8\n"
"addq	$1, 248(%%rax)\n"

"movq	256(%%rax), %%r8\n"
"addq	$1, 256(%%rax)\n"

"movq	264(%%rax), %%r8\n"
"addq	$1, 264(%%rax)\n"

"movq	272(%%rax), %%r8\n"
"addq	$1, 272(%%rax)\n"

"movq	280(%%rax), %%r8\n"
"addq	$1, 280(%%rax)\n"

"movq	288(%%rax), %%r8\n"
"addq	$1, 288(%%rax)\n"

"movq	296(%%rax), %%r8\n"
"addq	$1, 296(%%rax)\n"

"movq	304(%%rax), %%r8\n"
"addq	$1, 304(%%rax)\n"

"movq	312(%%rax), %%r8\n"
"addq	$1, 312(%%rax)\n"

"movq	320(%%rax), %%r8\n"
"addq	$1, 320(%%rax)\n"

"movq	328(%%rax), %%r8\n"
"addq	$1, 328(%%rax)\n"

"movq	336(%%rax), %%r8\n"
"addq	$1, 336(%%rax)\n"

"movq	344(%%rax), %%r8\n"
"addq	$1, 344(%%rax)\n"

"movq	352(%%rax), %%r8\n"
"addq	$1, 352(%%rax)\n"

"movq	360(%%rax), %%r8\n"
"addq	$1, 360(%%rax)\n"

"movq	368(%%rax), %%r8\n"
"addq	$1, 368(%%rax)\n"

"movq	376(%%rax), %%r8\n"
"addq	$1, 376(%%rax)\n"

"movq	384(%%rax), %%r8\n"
"addq	$1, 384(%%rax)\n"

"movq	392(%%rax), %%r8\n"
"addq	$1, 392(%%rax)\n"

"movq	400(%%rax), %%r8\n"
"addq	$1, 400(%%rax)\n"

"movq	408(%%rax), %%r8\n"
"addq	$1, 408(%%rax)\n"

"movq	416(%%rax), %%r8\n"
"addq	$1, 416(%%rax)\n"

"movq	424(%%rax), %%r8\n"
"addq	$1, 424(%%rax)\n"

"movq	432(%%rax), %%r8\n"
"addq	$1, 432(%%rax)\n"

"movq	440(%%rax), %%r8\n"
"addq	$1, 440(%%rax)\n"

"movq	448(%%rax), %%r8\n"
"addq	$1, 448(%%rax)\n"

"movq	456(%%rax), %%r8\n"
"addq	$1, 456(%%rax)\n"

"movq	464(%%rax), %%r8\n"
"addq	$1, 464(%%rax)\n"

"movq	472(%%rax), %%r8\n"
"addq	$1, 472(%%rax)\n"

"movq	480(%%rax), %%r8\n"
"addq	$1, 480(%%rax)\n"

"movq	488(%%rax), %%r8\n"
"addq	$1, 488(%%rax)\n"

"movq	496(%%rax), %%r8\n"
"addq	$1, 496(%%rax)\n"

"movq	504(%%rax), %%r8\n"
"addq	$1, 504(%%rax)\n"

"movq	512(%%rax), %%r8\n"
"addq	$1, 512(%%rax)\n"

"movq	520(%%rax), %%r8\n"
"addq	$1, 520(%%rax)\n"

"movq	528(%%rax), %%r8\n"
"addq	$1, 528(%%rax)\n"

"movq	536(%%rax), %%r8\n"
"addq	$1, 536(%%rax)\n"

"movq	544(%%rax), %%r8\n"
"addq	$1, 544(%%rax)\n"

"movq	552(%%rax), %%r8\n"
"addq	$1, 552(%%rax)\n"

"movq	560(%%rax), %%r8\n"
"addq	$1, 560(%%rax)\n"

"movq	568(%%rax), %%r8\n"
"addq	$1, 568(%%rax)\n"

"movq	576(%%rax), %%r8\n"
"addq	$1, 576(%%rax)\n"

"movq	584(%%rax), %%r8\n"
"addq	$1, 584(%%rax)\n"

"movq	592(%%rax), %%r8\n"
"addq	$1, 592(%%rax)\n"

"movq	600(%%rax), %%r8\n"
"addq	$1, 600(%%rax)\n"

"movq	608(%%rax), %%r8\n"
"addq	$1, 608(%%rax)\n"

"movq	616(%%rax), %%r8\n"
"addq	$1, 616(%%rax)\n"

"movq	624(%%rax), %%r8\n"
"addq	$1, 624(%%rax)\n"

"movq	632(%%rax), %%r8\n"
"addq	$1, 632(%%rax)\n"

"movq	640(%%rax), %%r8\n"
"addq	$1, 640(%%rax)\n"

"movq	648(%%rax), %%r8\n"
"addq	$1, 648(%%rax)\n"

"movq	656(%%rax), %%r8\n"
"addq	$1, 656(%%rax)\n"

"movq	664(%%rax), %%r8\n"
"addq	$1, 664(%%rax)\n"

"movq	672(%%rax), %%r8\n"
"addq	$1, 672(%%rax)\n"

"movq	680(%%rax), %%r8\n"
"addq	$1, 680(%%rax)\n"

"movq	688(%%rax), %%r8\n"
"addq	$1, 688(%%rax)\n"

"movq	696(%%rax), %%r8\n"
"addq	$1, 696(%%rax)\n"

"movq	704(%%rax), %%r8\n"
"addq	$1, 704(%%rax)\n"

"movq	712(%%rax), %%r8\n"
"addq	$1, 712(%%rax)\n"

"movq	720(%%rax), %%r8\n"
"addq	$1, 720(%%rax)\n"

"movq	728(%%rax), %%r8\n"
"addq	$1, 728(%%rax)\n"

"movq	736(%%rax), %%r8\n"
"addq	$1, 736(%%rax)\n"

"movq	744(%%rax), %%r8\n"
"addq	$1, 744(%%rax)\n"

"movq	752(%%rax), %%r8\n"
"addq	$1, 752(%%rax)\n"

"movq	760(%%rax), %%r8\n"
"addq	$1, 760(%%rax)\n"

"movq	768(%%rax), %%r8\n"
"addq	$1, 768(%%rax)\n"

"movq	776(%%rax), %%r8\n"
"addq	$1, 776(%%rax)\n"

"movq	784(%%rax), %%r8\n"
"addq	$1, 784(%%rax)\n"

"movq	792(%%rax), %%r8\n"
"addq	$1, 792(%%rax)\n"

"movq	800(%%rax), %%r8\n"
"addq	$1, 800(%%rax)\n"

"movq	808(%%rax), %%r8\n"
"addq	$1, 808(%%rax)\n"

"movq	816(%%rax), %%r8\n"
"addq	$1, 816(%%rax)\n"

"movq	824(%%rax), %%r8\n"
"addq	$1, 824(%%rax)\n"

"movq	832(%%rax), %%r8\n"
"addq	$1, 832(%%rax)\n"

"movq	840(%%rax), %%r8\n"
"addq	$1, 840(%%rax)\n"

"movq	848(%%rax), %%r8\n"
"addq	$1, 848(%%rax)\n"

"movq	856(%%rax), %%r8\n"
"addq	$1, 856(%%rax)\n"

"movq	864(%%rax), %%r8\n"
"addq	$1, 864(%%rax)\n"

"movq	872(%%rax), %%r8\n"
"addq	$1, 872(%%rax)\n"

"movq	880(%%rax), %%r8\n"
"addq	$1, 880(%%rax)\n"

"movq	888(%%rax), %%r8\n"
"addq	$1, 888(%%rax)\n"

"movq	896(%%rax), %%r8\n"
"addq	$1, 896(%%rax)\n"

"movq	904(%%rax), %%r8\n"
"addq	$1, 904(%%rax)\n"

"movq	912(%%rax), %%r8\n"
"addq	$1, 912(%%rax)\n"

"movq	920(%%rax), %%r8\n"
"addq	$1, 920(%%rax)\n"

"movq	928(%%rax), %%r8\n"
"addq	$1, 928(%%rax)\n"

"movq	936(%%rax), %%r8\n"
"addq	$1, 936(%%rax)\n"

"movq	944(%%rax), %%r8\n"
"addq	$1, 944(%%rax)\n"

"movq	952(%%rax), %%r8\n"
"addq	$1, 952(%%rax)\n"

"movq	960(%%rax), %%r8\n"
"addq	$1, 960(%%rax)\n"

"movq	968(%%rax), %%r8\n"
"addq	$1, 968(%%rax)\n"

"movq	976(%%rax), %%r8\n"
"addq	$1, 976(%%rax)\n"

"movq	984(%%rax), %%r8\n"
"addq	$1, 984(%%rax)\n"

"movq	992(%%rax), %%r8\n"
"addq	$1, 992(%%rax)\n"

"movq	1000(%%rax), %%r8\n"
"addq	$1, 1000(%%rax)\n"

"movq	1008(%%rax), %%r8\n"
"addq	$1, 1008(%%rax)\n"

"movq	1016(%%rax), %%r8\n"
"addq	$1, 1016(%%rax)\n"

"addq	$1024, %0\n"
: "=a" (ptr)
: "0" (ptr)
: "%r8");
