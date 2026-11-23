import time

def nod (a,b):
    start_time = time.time()
    r0, r1 = a, b
    x0, x1 = 1, 0
    y0, y1 = 0, 1
    i = 1
    while True:
        print(i, " : ", r1, x1, y1)
        chastnoe = r0 // r1
        ostatok = r0 % r1
        if (ostatok == 0):
            break
        if 2*abs(ostatok) > abs(r1):
            chastnoe += 1
            ostatok = r1*chastnoe - r0
        x0_old = x0
        y0_old = y0
        r0_old = r0
        r1_old = r1
            
        x0 = x1
        x1 = x0_old - chastnoe * x1
        
        y0 = y1
        y1 = y0_old - chastnoe * y1

        r0 = r1_old
        r1 = r0_old - chastnoe * r1
            
        i += 1

    if (r1 < 0):
        r1 = -r1
        x1 = -x1
        y1 = -y1

    end_time = time.time()
    print(f"Execution time: {end_time - start_time} seconds")
    return r1, x1, y1

print("1.")
print(nod(76442254787382549361, 10839626249153696837))

print("\n2.")
print(
    nod(
        1185424818577499562119347302107269112893,
        446596617388872800391921394077531729451,
    )
)

print("\n3.")
print(
    nod(
        16044312434096626538807819619243235254916564726186151279138828221183369568517841,
        97459036020809271505326192941985473863968504093300413836237675803378577076822423,
    )
)