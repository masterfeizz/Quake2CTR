 .386P
 .model FLAT
;
; r_parta.s
; x86 assembly-language particle code
;

include qasm.inc
include d_if.inc

if	id386

_DATA SEGMENT
_DATA ENDS

CONST SEGMENT
eight_thousand_hex dd 047000000r
PARTICLE_33     equ 0
PARTICLE_66     equ 1
PARTICLE_OPAQUE equ 2
CONST ENDS

_BSS SEGMENT
short_izi DW 01H DUP (?)
align 4
zi DD 01h DUP (?)
u DD 01H DUP (?)
v DD 01H DUP (?)
tmp DD 01H DUP (?)
transformed_vec DD 03H DUP (?)
local_vec DD 03H DUP (?)
ebpsave DD 01H DUP (?)
_BSS ENDS

_TEXT SEGMENT
 align 4
 public _R_DrawParticle
_R_DrawParticle:
;
; save trashed variables
;
 mov  dword ptr [ebpsave], ebp
 push esi
 push edi
 push ebx

;
; transform the particle
;
; VectorSubtract (pparticle->origin, r_origin, local);
 mov  esi, dword ptr [_partparms+partparms_particle]
 fld  dword ptr [esi+0]          ; p_o.x
 fsub dword ptr [_r_origin+0]     ; p_o.x-r_o.x
 fld  dword ptr [esi+4]          ; p_o.y | p_o.x-r_o.x
 fsub dword ptr [_r_origin+4]     ; p_o.y-r_o.y | p_o.x-r_o.x
 fld  dword ptr [esi+8]          ; p_o.z | p_o.y-r_o.y | p_o.x-r_o.x
 fsub dword ptr [_r_origin+8]     ; p_o.z-r_o.z | p_o.y-r_o.y | p_o.x-r_o.x
 fxch st(2)                      ; p_o.x-r_o.x | p_o.y-r_o.y | p_o.z-r_o.z
 fstp dword ptr [local_vec+0]        ; p_o.y-r_o.y | p_o.z-r_o.z
 fstp dword ptr [local_vec+4]        ; p_o.z-r_o.z
 fstp dword ptr [local_vec+8]        ; (empty)

; transformed[0] = DotProduct(local, r_pright);
; transformed[1] = DotProduct(local, r_pup);
; transformed[2] = DotProduct(local, r_ppn);
 fld  dword ptr [local_vec+0]        ; l.x
 fmul dword ptr [_r_pright+0]     ; l.x*pr.x
 fld  dword ptr [local_vec+4]        ; l.y | l.x*pr.x
 fmul dword ptr [_r_pright+4]     ; l.y*pr.y | l.x*pr.x
 fld  dword ptr [local_vec+8]        ; l.z | l.y*pr.y | l.x*pr.x
 fmul dword ptr [_r_pright+8]     ; l.z*pr.z | l.y*pr.y | l.x*pr.x
 fxch st(2)                      ; l.x*pr.x | l.y*pr.y | l.z*pr.z
 faddp st(1), st                 ; l.x*pr.x + l.y*pr.y | l.z*pr.z
 faddp st(1), st                 ; l.x*pr.x + l.y*pr.y + l.z*pr.z
 fstp  dword ptr [transformed_vec+0] ; (empty)

 fld  dword ptr [local_vec+0]        ; l.x
 fmul dword ptr [_r_pup+0]        ; l.x*pr.x
 fld  dword ptr [local_vec+4]        ; l.y | l.x*pr.x
 fmul dword ptr [_r_pup+4]        ; l.y*pr.y | l.x*pr.x
 fld  dword ptr [local_vec+8]        ; l.z | l.y*pr.y | l.x*pr.x
 fmul dword ptr [_r_pup+8]        ; l.z*pr.z | l.y*pr.y | l.x*pr.x
 fxch st(2)                      ; l.x*pr.x | l.y*pr.y | l.z*pr.z
 faddp st(1), st                 ; l.x*pr.x + l.y*pr.y | l.z*pr.z
 faddp st(1), st                 ; l.x*pr.x + l.y*pr.y + l.z*pr.z
 fstp  dword ptr [transformed_vec+4] ; (empty)

 fld  dword ptr [local_vec+0]        ; l.x
 fmul dword ptr [_r_ppn+0]        ; l.x*pr.x
 fld  dword ptr [local_vec+4]        ; l.y | l.x*pr.x
 fmul dword ptr [_r_ppn+4]        ; l.y*pr.y | l.x*pr.x
 fld  dword ptr [local_vec+8]        ; l.z | l.y*pr.y | l.x*pr.x
 fmul dword ptr [_r_ppn+8]        ; l.z*pr.z | l.y*pr.y | l.x*pr.x
 fxch st(2)                      ; l.x*pr.x | l.y*pr.y | l.z*pr.z
 faddp st(1), st(0)                 ; l.x*pr.x + l.y*pr.y | l.z*pr.z
 faddp st(1), st(0)                 ; l.x*pr.x + l.y*pr.y + l.z*pr.z
 fstp  dword ptr [transformed_vec+8] ; (empty)

;
; make sure that the transformed particle is not in front of
; the particle Z clip plane.  We can do the comparison in 
; integer space since we know the sign of one of the inputs
; and can figure out the sign of the other easily enough.
;
;	if (transformed[2] < PARTICLE_Z_CLIP)
;		return;

 mov  eax, dword ptr [transformed_vec+8]
 and  eax, eax
 js   endpartfunc
 cmp  eax, float_particle_z_clip
 jl   endpartfunc

;
; project the point by initiating the 1/z calc
;
;	zi = 1.0 / transformed[2];
 fld   float_1
 fdiv  dword ptr [transformed_vec+8]

; prefetch the next particle
 mov ebp, ds:dword ptr [_s_prefetch_address]
 mov ebp, [ebp]

; finish the above divide
 fstp  dword ptr [zi]

; u = (int)(xcenter + zi * transformed[0] + 0.5);
; v = (int)(ycenter - zi * transformed[1] + 0.5);
 fld   dword ptr [zi]                           ; zi
 fmul  dword ptr [transformed_vec+0]    ; zi * transformed[0]
 fld   dword ptr [zi]                           ; zi | zi * transformed[0]
 fmul  dword ptr [transformed_vec+4]    ; zi * transformed[1] | zi * transformed[0]
 fxch  st(1)                        ; zi * transformed[0] | zi * transformed[1]
 fadd  ds:dword ptr[_xcenter]                      ; xcenter + zi * transformed[0] | zi * transformed[1]
 fxch  st(1)                        ; zi * transformed[1] | xcenter + zi * transformed[0]
 fld   ds:dword ptr[_ycenter]                      ; ycenter | zi * transformed[1] | xcenter + zi * transformed[0]
 fsubrp st(1), st(0)                ; ycenter - zi * transformed[1] | xcenter + zi * transformed[0]
 fxch  st(1)                        ; xcenter + zi * transformed[0] | ycenter + zi * transformed[1]
 fadd  float_point5                   ; xcenter + zi * transformed[0] + 0.5 | ycenter - zi * transformed[1]
 fxch  st(1)                        ; ycenter - zi * transformed[1] | xcenter + zi * transformed[0] + 0.5 
 fadd  float_point5                   ; ycenter - zi * transformed[1] + 0.5 | xcenter + zi * transformed[0] + 0.5 
 fxch  st(1)                        ; u | v
 fistp dword ptr [u]                ; v
 fistp dword ptr [v]                ; (empty)

;
; clip out the particle
;

;	if ((v > d_vrectbottom_particle) || 
;		(u > d_vrectright_particle) ||
;		(v < d_vrecty) ||
;		(u < d_vrectx))
;	{
;		return;
;	}

 mov ebx, u
 mov ecx, v
 cmp ecx, ds:dword ptr [_d_vrectbottom_particle]
 jg  endpartfunc
 cmp ecx, ds:dword ptr [_d_vrecty]
 jl  endpartfunc
 cmp ebx, ds:dword ptr [_d_vrectright_particle]
 jg  endpartfunc
 cmp ebx, ds:dword ptr [_d_vrectx]
 jl  endpartfunc

;
; compute addresses of zbuffer, framebuffer, and 
; compute the Z-buffer reference value.
;
; EBX      = U
; ECX      = V
;
; Outputs:
; ESI = Z-buffer address
; EDI = framebuffer address
;
; ESI = d_pzbuffer + (d_zwidth * v) + u;
 mov esi, ds:dword ptr[_d_pzbuffer]             ; esi = d_pzbuffer
 mov eax, ds:dword ptr[_d_zwidth]               ; eax = d_zwidth
 mul ecx                         ; eax = d_zwidth*v
 add eax, ebx                    ; eax = d_zwidth*v+u
 shl eax, 1                      ; eax = 2*(d_zwidth*v+u)
 add esi, eax                    ; esi = ( short * ) ( d_pzbuffer + ( d_zwidth * v ) + u )

; initiate
; izi = (int)(zi * 0x8000);
 fld  dword ptr [zi]
 fmul eight_thousand_hex

; EDI = pdest = d_viewbuffer + d_scantable[v] + u;
 lea edi, ds:dword ptr _d_scantable[ecx*4]
 mov edi, dword ptr [edi]
 add edi, ds:dword ptr[_d_viewbuffer]
 add edi, ebx

; complete
; izi = (int)(zi * 0x8000);
 fistp dword ptr [tmp]
 mov eax, dword ptr [tmp]
 mov word ptr [short_izi], ax

;
; determine the screen area covered by the particle,
; which also means clamping to a min and max
;
;	pix = izi >> d_pix_shift;
 xor edx, edx
 mov dx, word ptr [short_izi]
 mov ecx, ds:dword ptr[_d_pix_shift]
 shr dx, cl

;	if (pix < d_pix_min)
;		pix = d_pix_min;
 cmp edx, ds:dword ptr[_d_pix_min]
 jge check_pix_max
 mov edx, ds:dword ptr[_d_pix_min]
 jmp skip_pix_clamp

;	else if (pix > d_pix_max)
;		pix = d_pix_max;
check_pix_max:
 cmp edx, ds:dword ptr[_d_pix_max]
 jle skip_pix_clamp
 mov edx, ds:dword ptr[_d_pix_max]

skip_pix_clamp:

;
; render the appropriate pixels
;
; ECX = count (used for inner loop)
; EDX = count (used for outer loop)
; ESI = zbuffer
; EDI = framebuffer
;
 mov ecx, edx

 cmp ecx, 1
 ja  over

over:

;
; at this point:
;
; ECX = count
;
 push ecx
 push edi
 push esi

top_of_pix_vert_loop:

top_of_pix_horiz_loop:

;	for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
;	{
;		for (i=0 ; i<pix ; i++)
;		{
;			if (pz[i] <= izi)
;			{
;				pdest[i] = blendparticle( color, pdest[i] );
;			}
;		}
;	}
 xor   eax, eax

 mov   ax, word ptr [esi]

 cmp   ax, word ptr [short_izi]
 jg    end_of_horiz_loop

 mov   eax, ds:dword ptr [_partparms+partparms_color]

 cmp ds:dword ptr [_partparms+partparms_level], PARTICLE_66
 je  blendfunc_66
 jl  blendfunc_33
; BlendParticle100
 mov byte ptr [edi], al
 jmp done_blending
blendfunc_33:
 mov ebp, ds:dword ptr [_vid+vid_alphamap]
 xor ebx, ebx

 mov bl,  byte ptr [edi]
 shl ebx, 8

 add ebp, ebx
 add ebp, eax

 mov al,  byte ptr [ebp]

 mov byte ptr [edi], al
 jmp done_blending
blendfunc_66:
 mov ebp, ds:dword ptr [_vid+vid_alphamap]
 xor ebx, ebx

 shl eax,  8
 mov bl,   byte ptr [edi]

 add ebp, ebx
 add ebp, eax

 mov al,  byte ptr [ebp]

 mov byte ptr [edi], al
done_blending:

 add   edi, 1
 add   esi, 2

end_of_horiz_loop:

 dec   ecx
 jnz   top_of_pix_horiz_loop

 pop   esi
 pop   edi

 mov   ebp, ds:dword ptr[_d_zwidth]
 shl   ebp, 1

 add   esi, ebp
 add   edi, ds:dword ptr [_r_screenwidth]

 pop   ecx
 push  ecx

 push  edi
 push  esi

 dec   edx
 jnz   top_of_pix_vert_loop

 pop   ecx
 pop   ecx
 pop   ecx

endpartfunc:
 pop ebx
 pop edi
 pop esi
 mov ebp, dword ptr[ebpsave]
 ret

_TEXT ENDS
endif	;id386
 END
