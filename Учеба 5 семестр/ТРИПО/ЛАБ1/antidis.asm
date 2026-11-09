.code


antidis_add1 PROC
    mov eax, ecx         

    xor edx, edx          
    jz  real_code         
    jnz real_code      

    db 0E8h               

real_code:
    add eax, 1            
    ret
antidis_add1 ENDP

antidis_jmid PROC
    mov eax, ecx          

  
    db 0EBh, 001h  ;short jump+1         

    db 0C7h             
                            
   
    add eax, 3              
    ret
antidis_jmid ENDP

END