.alias  inPos   v0
.alias  inTex   v1
.alias  inOff   v2

.out    outPos  position
.out    outTex  texcoord0

.fvec   projection[4]
.fvec   view[4]

.constf myconst(0.0, 1.0, 0.0, 0.0)
.alias  one     myconst.yyyy

.entry main
.proc main
    mov r0.xyz, inPos
    mov r0.w,   one

    dp4 r1.x, view[0], r0
    dp4 r1.y, view[1], r0
    dp4 r1.z, view[2], r0
    dp4 r1.w, view[3], r0

    add r1.xy, r1.xy, inOff.xy

    dp4 outPos.x, projection[0], r1
    dp4 outPos.y, projection[1], r1
    dp4 outPos.z, projection[2], r1
    dp4 outPos.w, projection[3], r1

    mov outTex, inTex
    end
.end