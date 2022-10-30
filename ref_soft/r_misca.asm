 .386P
 .model FLAT
;
; r_miscas.s
; x86 assembly-language transform vector code
;

include qasm.inc
include d_if.inc

if	id386

_DATA SEGMENT
vin		equ		4
vout	equ		8
_DATA ENDS

_TEXT SEGMENT

;----------------------------------------------------------------------
; transform vector code
;----------------------------------------------------------------------
 align 4
 public _TransformVector
_TransformVector:
 mov eax, ds:dword ptr [vin+esp]
 mov edx, ds:dword ptr [vout+esp]

 fld  dword ptr [eax+0]
 fmul dword ptr [_vright+0]
 fld  dword ptr [eax+0]
 fmul dword ptr [_vup+0]
 fld  dword ptr [eax+0]
 fmul dword ptr [_vpn+0]

 fld  dword ptr [eax+4]
 fmul dword ptr [_vright+4]
 fld  dword ptr [eax+4]
 fmul dword ptr [_vup+4]
 fld  dword ptr [eax+4]
 fmul dword ptr [_vpn+4]

 fxch st(2)

 faddp st(5), st(0)
 faddp st(3), st(0)
 faddp st(1), st(0)

 fld  dword ptr [eax+8]
 fmul dword ptr [_vright+8]
 fld  dword ptr [eax+8]
 fmul dword ptr [_vup+8]
 fld  dword ptr [eax+8]
 fmul dword ptr [_vpn+8]

 fxch st(2)

 faddp st(5), st(0)
 faddp st(3), st(0)
 faddp st(1), st(0)

 fstp dword ptr [edx+8]
 fstp dword ptr [edx+4]
 fstp dword ptr [edx+0]
 ret
_TEXT ENDS
endif	;id386
 END
