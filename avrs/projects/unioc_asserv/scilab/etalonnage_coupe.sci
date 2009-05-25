pi=3.1415927

// +0� +400mm
A=[
30110 -17327 -30676 -17198 972 35188;
30121 -17262 -30676 -17180 988 35070;
30080 -17148 -30589 -17169 992 34993;
30050 -17104 -30566 -17206 958 35066;
30071 -17089 -30562 -17215 945 35073;
29973 -17062 -30525 -17223 985 35106
];

// +120� +400mm
B=[
-30364 -17508 582 35154 30863 -18085;
-30366 -17522 600 35165 30862 -18091;
-30435 -17475 656 35248 30930 -18111;
-30519 -17591 687 35398 30975 -18141;
-30317 -17514 608 35145 30774 -17957;
-30438 -17622 594 35218 30855 -18048
];

// +240� +400mm
C=[
500 35160 30258 -18057 -30970 -17518;
497 35280 30253 -18123 -31100 -17467;
471 35144 30160 -18043 -30946 -17489;
483 35298 30132 -18040 -30953 -17432;
457 35435 30290 -18021 -31051 -17561;
499 35295 30201 -18015 -30963 -17417
];

// Rot +360�
R=[
-1919 45131 -1050 45984 -590 44989;
-1668 45349 -964 45159 -752 45664;
-2050 45202 -292 45203 -870 45048;
-1731 45106 -913 45517 -712 45487;
-1642 45208 -789 45676 -724 45706;
-1797 44645 -603 44921 -456 45149
];

u_a = [400 0 0];
u_b = [400*cos(2*pi/3) 400*sin(2*pi/3) 0];
u_c = [400*cos(4*pi/3) 400*sin(4*pi/3) 0];
u_r1 = [0 0 2*pi*(1379/1700)];
u_r2 = [0 0 2*pi*(1374/1700)];
u_r3 = [0 0 2*pi*(1364/1700)];
u_r4 = [0 0 2*pi*(1381/1700)];
u_r5 = [0 0 2*pi*(1363/1700)];
u_r6 = [0 0 2*pi*(1374/1700)];

Moutput = [A;B;C;R];
Minput = [u_a;u_a;u_a;u_a;u_a;u_a;
          u_b;u_b;u_b;u_b;u_b;u_b;
          u_c;u_c;u_c;u_c;u_c;u_c;
          u_r1;u_r2;u_r3;u_r4;u_r5;u_r6];

[arc,la,lb,sig,resid] = armax( 0, 0, Moutput', Minput');

[Ax,Bx,Dx]=arma2p(arc);   //Results in polynomial form. 

M = pinv(coeff(Bx));

M         
