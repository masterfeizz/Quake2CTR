 .386P
 .model FLAT
;
; r_aliasa.s
; x86 assembly-language alias drawing code
;

include qasm.inc
include d_if.inc

if id386

_DATA SEGMENT
FALIAS_Z_CLIP_PLANE DD 040800000r
PS_SCALE DD 040800000r

newv = 20
oldv = 16
fv = 12
numpoints = 8
lerped_vert = -12
lightcos = -16
one = -20
_byte_to_dword_ptr_var = -24
zi_loc = -28
tmpint = -32
_DATA ENDS

_TEXT SEGMENT
 align 4
 public _R_AliasTransformFinalVerts
_R_AliasTransformFinalVerts:
 push ebp
 mov ebp, esp
 sub esp, 32
 push ebx
 push esi
 push edi

 fld1
 fstp dword ptr one[ebp]
 mov ecx, dword ptr numpoints[ebp]

top_of_loop:
;	/*
;	lerped_vert[0] = r_lerp_move[0] + oldv->v[0]*r_lerp_backv[0] + newv->v[0]*r_lerp_frontv[0];
;	lerped_vert[1] = r_lerp_move[1] + oldv->v[1]*r_lerp_backv[1] + newv->v[1]*r_lerp_frontv[1];
;	lerped_vert[2] = r_lerp_move[2] + oldv->v[2]*r_lerp_backv[2] + newv->v[2]*r_lerp_frontv[2];
;	*/

 mov esi, dword ptr oldv[ebp]
 mov edi, dword ptr newv[ebp]

 xor ebx, ebx

 mov bl, byte ptr [esi+DTRIVERTX_V0]
 mov dword ptr _byte_to_dword_ptr_var[ebp], ebx
 fild dword ptr _byte_to_dword_ptr_var[ebp]
 fmul dword ptr _r_lerp_backv                              ; oldv[0]*rlb[0]

 mov bl, byte ptr [esi+DTRIVERTX_V1]
 mov dword ptr _byte_to_dword_ptr_var[ebp], ebx
 fild dword ptr _byte_to_dword_ptr_var[ebp]
 fmul dword ptr _r_lerp_backv+4                            ; oldv[1]*rlb[1] | oldv[0]*rlb[0]

 mov bl, byte ptr [esi+DTRIVERTX_V2]
 mov dword ptr _byte_to_dword_ptr_var[ebp], ebx
 fild dword ptr _byte_to_dword_ptr_var[ebp]
 fmul dword ptr _r_lerp_backv+8                            ; oldv[2]*rlb[2] | oldv[1]*rlb[1] | oldv[0]*rlb[0]

 mov bl, byte ptr [edi+DTRIVERTX_V0]
 mov dword ptr _byte_to_dword_ptr_var[ebp], ebx
 fild dword ptr _byte_to_dword_ptr_var[ebp]
 fmul dword ptr _r_lerp_frontv                             ; newv[0]*rlf[0] | oldv[2]*rlb[2] | oldv[1]*rlb[1] | oldv[0]*rlb[0]

 mov bl, byte ptr [edi+DTRIVERTX_V1]
 mov dword ptr _byte_to_dword_ptr_var[ebp], ebx
 fild dword ptr _byte_to_dword_ptr_var[ebp]
 fmul dword ptr _r_lerp_frontv+4                           ; newv[1]*rlf[1] | newv[0]*rlf[0] | oldv[2]*rlb[2] | oldv[1]*rlb[1] | oldv[0]*rlb[0]

 mov bl, byte ptr [edi+DTRIVERTX_V2]
 mov dword ptr _byte_to_dword_ptr_var[ebp], ebx
 fild dword ptr _byte_to_dword_ptr_var[ebp]
 fmul dword ptr _r_lerp_frontv+8                           ; newv[2]*rlf[2] | newv[1]*rlf[1] | newv[0]*rlf[0] | oldv[2]*rlb[2] | oldv[1]*rlb[1] | oldv[0]*rlb[0]

 fxch st(5)                                                ; oldv[0]*rlb[0] | newv[1]*rlf[1] | newv[0]*rlf[0] | oldv[2]*rlb[2] | oldv[1]*rlb[1] | newv[2]*rlf[2]
 faddp st(2), st(0)                                        ; newv[1]*rlf[1] | oldv[0]*rlb[0] + newv[0]*rlf[0] | oldv[2]*rlb[2] | oldv[1]*rlb[1] | newv[2]*rlf[2]
 faddp st(3), st(0)                                        ; oldv[0]*rlb[0] + newv[0]*rlf[0] | oldv[2]*rlb[2] | oldv[1]*rlb[1] + newv[1]*rlf[1] | newv[2]*rlf[2]
 fxch st(1)                                                ; oldv[2]*rlb[2] | oldv[0]*rlb[0] + newv[0]*rlf[0] | oldv[1]*rlb[1] + newv[1]*rlf[1] | newv[2]*rlf[2]
 faddp st(3), st(0)                                        ; oldv[0]*rlb[0] + newv[0]*rlf[0] | oldv[1]*rlb[1] + newv[1]*rlf[1] | oldv[2]*rlb[2] + newv[2]*rlf[2]
 fadd dword ptr _r_lerp_move                               ; lv0 | oldv[1]*rlb[1] + newv[1]*rlf[1] | oldv[2]*rlb[2] + newv[2]*rlf[2]
 fxch st(1)                                                ; oldv[1]*rlb[1] + newv[1]*rlf[1] | lv0 | oldv[2]*rlb[2] + newv[2]*rlf[2]
 fadd dword ptr _r_lerp_move+4                             ; lv1 | lv0 | oldv[2]*rlb[2] + newv[2]*rlf[2]
 fxch st(2)                                                ; oldv[2]*rlb[2] + newv[2]*rlf[2] | lv0 | lv1
 fadd dword ptr _r_lerp_move+8                             ; lv2 | lv0 | lv1
 fxch st(1)                                                ; lv0 | lv2 | lv1
 fstp dword ptr lerped_vert[ebp]                           ; lv2 | lv1
 fstp dword ptr lerped_vert[ebp+8]                         ; lv2
 fstp dword ptr lerped_vert[ebp+4]                         ; (empty)

 mov eax, dword ptr _currententity
 mov eax, dword ptr [eax+ENTITY_FLAGS]
 mov ebx, 203776                                           ; RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM
 and eax, ebx
 je SHORT not_powersuit

;	/*
;	**    lerped_vert[0] += lightnormal[0] * POWERSUIT_SCALE
;	**    lerped_vert[1] += lightnormal[1] * POWERSUIT_SCALE
;	**    lerped_vert[2] += lightnormal[2] * POWERSUIT_SCALE
;	*/

 xor ebx, ebx
 mov bl, byte ptr [edi+DTRIVERTX_LNI]
 mov eax, 12
 mul ebx
 lea eax, dword ptr _r_avertexnormals[eax]

 fld dword ptr [eax]                                       ; n[0]
 fmul dword ptr PS_SCALE                                   ; n[0] * PS
 fld dword ptr [eax+4]                                     ; n[1] | n[0] * PS
 fmul dword ptr PS_SCALE                                   ; n[1] * PS | n[0] * PS
 fld dword ptr [eax+8]                                     ; n[2] | n[1] * PS | n[0] * PS
 fmul dword ptr PS_SCALE                                   ; n[2] * PS | n[1] * PS | n[0] * PS
 fld dword ptr lerped_vert[ebp]                            ; lv0 | n[2] * PS | n[1] * PS | n[0] * PS
 faddp st(3), st(0)                                        ; n[2] * PS | n[1] * PS | n[0] * PS + lv0
 fld dword ptr lerped_vert[ebp+4]                          ; lv1 | n[2] * PS | n[1] * PS | n[0] * PS + lv0
 faddp st(2), st(0)                                        ; n[2] * PS | n[1] * PS + lv1 | n[0] * PS + lv0
 fadd dword ptr lerped_vert[ebp+8]                         ; n[2] * PS + lv2 | n[1] * PS + lv1 | n[0] * PS + lv0
 fxch st(2)                                                ; LV0 | LV1 | LV2
 fstp dword ptr lerped_vert[ebp]                           ; LV1 | LV2
 fstp dword ptr lerped_vert[ebp+4]                         ; LV2
 fstp dword ptr lerped_vert[ebp+8]                         ; (empty)

not_powersuit:
;	/*
;	fv->flags = 0;
;
;	fv->xyz[0] = DotProduct(lerped_vert, aliastransform[0]) + aliastransform[0][3];
;	fv->xyz[1] = DotProduct(lerped_vert, aliastransform[1]) + aliastransform[1][3];
;	fv->xyz[2] = DotProduct(lerped_vert, aliastransform[2]) + aliastransform[2][3];
;	*/

 mov eax, dword ptr fv[ebp]
 mov dword ptr [eax+FINALVERT_FLAGS], 0

 fld dword ptr lerped_vert[ebp]                            ; lv0
 fmul dword ptr _aliastransform                            ; lv0*at[0][0]
 fld dword ptr lerped_vert[ebp+4]                          ; lv1 | lv0*at[0][0]
 fmul dword ptr _aliastransform+4                          ; lv1*at[0][1] | lv0*at[0][0]
 fld dword ptr lerped_vert[ebp+8]                          ; lv2 | lv1*at[0][1] | lv0*at[0][0]
 fmul dword ptr _aliastransform+8                          ; lv2*at[0][2] | lv1*at[0][1] | lv0*at[0][0]
 fxch st(2)                                                ; lv0*at[0][0] | lv1*at[0][1] | lv2*at[0][2]
 faddp st(1), st(0)                                        ; lv0*at[0][0] + lv1*at[0][1] | lv2*at[0][2]
 faddp st(1), st(0)                                        ; lv0*at[0][0] + lv1*at[0][1] + lv2*at[0][2]
 fadd dword ptr _aliastransform+12                         ; FV.X

 fld dword ptr lerped_vert[ebp]                            ; lv0
 fmul dword ptr _aliastransform+16                         ; lv0*at[1][0]
 fld dword ptr lerped_vert[ebp+4]                          ; lv1 | lv0*at[1][0]
 fmul dword ptr _aliastransform+20                         ; lv1*at[1][1] | lv0*at[1][0]
 fld dword ptr lerped_vert[ebp+8]                          ; lv2 | lv1*at[1][1] | lv0*at[1][0]
 fmul dword ptr _aliastransform+24                         ; lv2*at[1][2] | lv1*at[1][1] | lv0*at[1][0]
 fxch st(2)                                                ; lv0*at[1][0] | lv1*at[1][1] | lv2*at[1][2]
 faddp st(1), st(0)                                        ; lv0*at[1][0] + lv1*at[1][1] | lv2*at[1][2]
 faddp st(1), st(0)                                        ; lv0*at[1][0] + lv1*at[1][1] + lv2*at[1][2]
 fadd dword ptr _aliastransform+28                         ; FV.Y | FV.X
 fxch st(1)                                                ; FV.X | FV.Y
 fstp dword ptr [eax+FINALVERT_X]                          ; FV.Y

 fld dword ptr lerped_vert[ebp]                            ; lv0
 fmul dword ptr _aliastransform+32                         ; lv0*at[2][0]
 fld dword ptr lerped_vert[ebp+4]                          ; lv1 | lv0*at[2][0]
 fmul dword ptr _aliastransform+36                         ; lv1*at[2][1] | lv0*at[2][0]
 fld dword ptr lerped_vert[ebp+8]                          ; lv2 | lv1*at[2][1] | lv0*at[2][0]
 fmul dword ptr _aliastransform+40                         ; lv2*at[2][2] | lv1*at[2][1] | lv0*at[2][0]
 fxch st(2)                                                ; lv0*at[2][0] | lv1*at[2][1] | lv2*at[2][2]
 faddp st(1), st(0)                                        ; lv0*at[2][0] + lv1*at[2][1] | lv2*at[2][2]
 faddp st(1), st(0)                                        ; lv0*at[2][0] + lv1*at[2][1] + lv2*at[2][2]
 fadd dword ptr _aliastransform+44                         ; FV.Z | FV.Y
 fxch st(1)                                                ; FV.Y | FV.Z
 fstp dword ptr [eax+FINALVERT_Y]                          ; FV.Z
 fstp dword ptr [eax+FINALVERT_Z]                          ; (empty)

; /*
; **	lighting
; **
; **	plightnormal = r_avertexnormals[newv->lightnormalindex];
; **	lightcos = DotProduct (plightnormal, r_plightvec);
; **	temp = r_ambientlight;
; */

 xor ebx, ebx
 mov bl, byte ptr [edi+DTRIVERTX_LNI]
 mov eax, 12
 mul ebx
 lea eax, dword ptr _r_avertexnormals[eax]
 lea ebx, offset _r_plightvec

 fld dword ptr [eax]
 fmul dword ptr [ebx]
 fld dword ptr [eax+4]
 fmul dword ptr [ebx+4]
 fld dword ptr [eax+8]
 fmul dword ptr [ebx+8]
 fxch st(2)
 faddp st(1), st(0)
 faddp st(1), st(0)
 fstp dword ptr lightcos[ebp]
 mov eax, dword ptr lightcos[ebp]
 mov ebx, dword ptr _r_ambientlight

;	/*
;	if (lightcos < 0)
;	{
;		temp += (int)(r_shadelight * lightcos);
;
;		// clamp; because we limited the minimum ambient and shading light, we
;		// don't have to clamp low light, just bright
;		if (temp < 0)
;			temp = 0;
;	}
;
;	fv->v[4] = temp;
;	*/

 or eax, eax
 jns store_fv4

 fld dword ptr _r_shadelight
 fmul dword ptr lightcos[ebp]
 fistp dword ptr tmpint[ebp]
 add ebx, dword ptr tmpint[ebp]

 or ebx, ebx
 jns store_fv4

 mov ebx, 0
store_fv4:
 mov edi, dword ptr fv[ebp]
 mov dword ptr [edi+FINALVERT_V4], ebx

 mov edx, dword ptr [edi+FINALVERT_FLAGS]

;	/*
;	** do clip testing and projection here
;	*/
;	/*
;	if ( dest_vert->xyz[2] < ALIAS_Z_CLIP_PLANE )
;	{
;		dest_vert->flags |= ALIAS_Z_CLIP;
;	}
;	else
;	{
;		R_AliasProjectAndClipTestFinalVert( dest_vert );
;	}
;	*/

 mov eax, dword ptr [edi+FINALVERT_Z]
 and eax, eax
 js alias_z_clip
 cmp eax, dword ptr FALIAS_Z_CLIP_PLANE
 jl alias_z_clip

;	/*
;	This is the code to R_AliasProjectAndClipTestFinalVert
;
;	float zi;
;	float x, y, z;
;
;	x = fv->xyz[0];
;	y = fv->xyz[1];
;	z = fv->xyz[2];
;	zi = 1.0 / z;
;
;	fv->v[5] = zi * s_ziscale;
;
;	fv->v[0] = (x * aliasxscale * zi) + aliasxcenter;
;	fv->v[1] = (y * aliasyscale * zi) + aliasycenter;
;	*/

 fld dword ptr one[ebp]
 fdiv dword ptr [edi+FINALVERT_Z]                          ; zi

 mov eax, dword ptr [edi+32]
 mov eax, dword ptr [edi+64]

 fst dword ptr zi_loc[ebp]                                 ; zi
 fmul dword ptr _s_ziscale                                 ; fv5
 fld dword ptr [edi+FINALVERT_X]                           ; x | fv5
 fmul dword ptr _aliasxscale                               ; x * aliasxscale | fv5
 fld dword ptr [edi+FINALVERT_Y]                           ; y | x * aliasxscale | fv5
 fmul dword ptr _aliasyscale                               ; y * aliasyscale | x * aliasxscale | fv5
 fxch st(1)                                                ; x * aliasxscale | y * aliasyscale | fv5
 fmul dword ptr zi_loc[ebp]                                ; x * asx * zi | y * asy | fv5
 fadd dword ptr _aliasxcenter                              ; fv0 | y * asy | fv5
 fxch st(1)                                                ; y * asy | fv0 | fv5
 fmul dword ptr zi_loc[ebp]                                ; y * asy * zi | fv0 | fv5
 fadd dword ptr _aliasycenter                              ; fv1 | fv0 | fv5
 fxch st(2)                                                ; fv5 | fv0 | fv1
 fistp dword ptr [edi+FINALVERT_V5]                        ; fv0 | fv1
 fistp dword ptr [edi+FINALVERT_V0]                        ; fv1
 fistp dword ptr [edi+FINALVERT_V1]                        ; (empty)

;	/*
;	if (fv->v[0] < r_refdef.aliasvrect.x)
;		fv->flags |= ALIAS_LEFT_CLIP;
;	if (fv->v[1] < r_refdef.aliasvrect.y)
;		fv->flags |= ALIAS_TOP_CLIP;
;	if (fv->v[0] > r_refdef.aliasvrectright)
;		fv->flags |= ALIAS_RIGHT_CLIP;
;	if (fv->v[1] > r_refdef.aliasvrectbottom)
;		fv->flags |= ALIAS_BOTTOM_CLIP;
;	*/

 mov eax, dword ptr [edi+FINALVERT_V0]
 mov ebx, dword ptr [edi+FINALVERT_V1]

 cmp eax, dword ptr _r_refdef+20
 jge ct_alias_top
 or edx, ALIAS_LEFT_CLIP

ct_alias_top:
 cmp ebx, dword ptr _r_refdef+24                           ; r_refdef.aliasvrect.y
 jge ct_alias_right
 or edx, ALIAS_TOP_CLIP

ct_alias_right:
 cmp eax, dword ptr _r_refdef+48                           ; r_refdef.aliasvrectright
 jle ct_alias_bottom
 or edx, ALIAS_RIGHT_CLIP

ct_alias_bottom:
 cmp ebx, dword ptr _r_refdef+52                           ; r_refdef.aliasvrectbottom
 jle end_of_loop
 or edx, ALIAS_BOTTOM_CLIP

 jmp end_of_loop

alias_z_clip:
 or edx, ALIAS_Z_CLIP

end_of_loop:
 mov dword ptr [edi+FINALVERT_FLAGS], edx
 add dword ptr oldv[ebp], DTRIVERTX_SIZE
 add dword ptr newv[ebp], DTRIVERTX_SIZE
 add dword ptr fv[ebp], FINALVERT_SIZE

 dec ecx
 jne top_of_loop

 pop edi
 pop esi
 pop ebx
 mov esp, ebp
 pop ebp
 ret

_TEXT ENDS
endif ;id386
 END
