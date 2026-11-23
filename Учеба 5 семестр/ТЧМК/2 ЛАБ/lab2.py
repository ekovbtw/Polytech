import math

def ferma_test(n, a):
    if math.gcd(a, n) != 1:
        return "составное"

    if pow(a, n - 1, n) == 1:
        return "псевдопростое"
    else:
        return "составное"

def jacobi(a, n):
    a = a % n
    result = 1
    while a != 0:
        while a % 2 == 0:
            a = a // 2
            if n % 8 == 3 or n % 8 == 5:
                result = -result
        a, n = n, a
        if a % 4 == 3 and n % 4 == 3:
            result = -result
        a = a % n   
    if n == 1:
        return result
    return 0


def solovay_strassen_test(n, a):

    if math.gcd(a, n) != 1:
        return "составное"

    A = pow(a, (n - 1) // 2, n)
    B = jacobi(a, n) % n

    if A == B:
        return "псевдопростое"
    else:
        return "составное"


def miller_rabin_test(n, a):

    if math.gcd(a, n) != 1:
        return "составное"


    d = n - 1
    s = 0
    while d % 2 == 0:
        d //= 2
        s += 1

    x = pow(a, d, n)
    if x == 1 or x == n - 1:
        return "псевдопростое"

    for _ in range(s - 1):
        x = (x * x) % n
        if x == n - 1:
            return "псевдопростое"

    return "составное"


def cond_ferma(n, a):
    """Строка показа условия Ферма для одного основания."""
    if math.gcd(a, n) != 1:
        return f"НОД({a},n)={math.gcd(a,n)}>1 -> делитель найден"
    val = pow(a, n - 1, n)
    mark = "= 1 (выполнено)" if val == 1 else "!= 1 (НАРУШЕНО)"
    return f"a^(n-1) mod n = {val}  {mark}"


def cond_solovay(n, a):
    """Строка показа условия Соловея-Штрассена для одного основания."""
    if math.gcd(a, n) != 1:
        return f"НОД({a},n)={math.gcd(a,n)}>1 -> делитель найден"
    A = pow(a, (n - 1) // 2, n)
    j = jacobi(a, n)
    B = j % n
    mark = "A = B (выполнено)" if A == B else "A != B (НАРУШЕНО)"
    return f"A = a^((n-1)/2) mod n = {A}; (a/n) = {j}, B = {B}  {mark}"


def cond_miller(n, a):
    """Строка показа условия Рабина-Миллера для одного основания."""
    if math.gcd(a, n) != 1:
        return f"НОД({a},n)={math.gcd(a,n)}>1 -> делитель найден"
    d = n - 1
    s = 0
    while d % 2 == 0:
        d //= 2
        s += 1
    x = pow(a, d, n)
    seq = [x]
    if x == 1 or x == n - 1:
        return f"a^d mod n = {x} = {'1' if x==1 else 'n-1'} (выполнено, s={s})"
    for _ in range(s - 1):
        x = (x * x) % n
        seq.append(x)
        if x == n - 1:
            chain = ", ".join(str(v) for v in seq)
            return f"цепочка [{chain}] -> n-1 (выполнено, s={s})"
    chain = ", ".join(str(v) for v in seq)
    return f"цепочка [{chain}] -> нет ни 1, ни n-1 (НАРУШЕНО, s={s})"


def run_test(n, test, cond, f):
    """Прогон одного теста по основаниям, печать в требуемом формате."""
    bases = [2, 3, 4, 5, 6, 7, 11, 13, 17, 19, 23, 29, 31, 37]
    witness = None
    for a in bases:
        if test(n, a) == "составное":
            witness = a
            break

    if witness is None:
        verdict = "псевдопростое"
        f.write(f"{verdict}\n")
        good = [a for a in bases if math.gcd(a, n) == 1][:5]
        for a in good:
            f.write(f"    a = {a}: {cond(n, a)}\n")
    else:
        verdict = "составное"
        f.write(f"{verdict}\n")
        f.write(f"    основание a = {witness}: {cond(n, witness)}\n")
    return verdict


if __name__ == "__main__":
    numbers = {
        1: 10493450440980479677,
        2: 1345660142737584259622907034936260521041,
        3: 306910024509079518200785821965909785693,
        4: 39870070437539877234362471636698683656603034967102337868614604715867678153130419,
    }

    tests = [
        (ferma_test,            "Тест Ферма",             cond_ferma),
        (solovay_strassen_test, "Тест Соловея-Штрассена", cond_solovay),
        (miller_rabin_test,     "Тест Рабина-Миллера",    cond_miller),
    ]

    with open("results.txt", "w", encoding="utf-8") as f:
        for i, n in numbers.items():
            f.write(f"Число {i} ({len(str(n))} знаков): n = {n}\n")
            verdicts = []
            for test, name, cond in tests:
                f.write(f"  {name}: ")
                v = run_test(n, test, cond, f)
                verdicts.append(v)
            # итог по числу
            if all(v == "псевдопростое" for v in verdicts):
                itog = "псевдопростое"
            else:
                itog = "составное"
            f.write(f"  ИТОГ по числу {i}: {itog}\n")
            f.write("\n")

    print("Готово -> results.txt")