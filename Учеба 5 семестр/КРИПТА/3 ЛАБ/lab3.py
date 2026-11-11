import math
import time
from collections import deque


NUMBERS = [
    20883822746071683169,
    1722519516032430644292873814419911867131,
    8920650994859148078591917235227480860194094575124212498116922267113685281496797,
]

TIME_LIMIT = 10       # Секунд на один метод для одного числа
BASE_BOUND = 2000    # Граница базы для решета и непрерывных дробей


# ---------- Общие вспомогательные функции ----------

def primes_up_to(bound):
    """Решето Эратосфена."""
    sieve = bytearray(b"\x01") * (bound + 1)
    sieve[:2] = b"\x00\x00"

    for p in range(2, math.isqrt(bound) + 1):
        if sieve[p]:
            count = (bound - p * p) // p + 1
            sieve[p * p::p] = b"\x00" * count

    return [p for p in range(2, bound + 1) if sieve[p]]


def simple_divisor(n):
    """Обработка чётных чисел и полных квадратов."""
    if n % 2 == 0:
        return 2 if n > 2 else None

    root = math.isqrt(n)
    if root * root == n and root > 1:
        return root

    return None


def make_base(n, bound):
    """
    База: -1, 2 и нечётные простые p,
    для которых n является квадратичным вычетом mod p.

    Второе возвращаемое значение — делитель,
    если он обнаружен при построении базы.
    """
    base = [-1]

    for p in primes_up_to(bound):
        if n % p == 0:
            if p < n:
                return base, p
            continue

        if p == 2 or pow(n, (p - 1) // 2, p) == 1:
            base.append(p)

    return base, None


def smooth_factorization(value, base):
    """
    Возвращает вектор показателей, если value гладко над base.
    Иначе возвращает None. Координата 0 соответствует -1.
    """
    if value == 0:
        return None

    remainder = abs(value)
    exponents = [int(value < 0)]

    for p in base[1:]:
        exponent = 0

        while remainder % p == 0:
            remainder //= p
            exponent += 1

        exponents.append(exponent)

    return exponents if remainder == 1 else None


class RelationCollector:
    """Соотношения и исключение Гаусса над GF(2)."""

    def __init__(self, n, base):
        self.n = n
        self.base = base
        self.rows = []
        self.pivots = {}
        self.dependencies = 0

    def add(self, left, value, exponents):
        """
        Добавляет left² ≡ value mod n.
        При успехе возвращает (делитель, s, t).
        """
        assert (left * left - value) % self.n == 0

        index = len(self.rows)
        self.rows.append((left, value, exponents))

        # Биты parity — чётности показателей.
        parity = sum(
            (exponent % 2) << j
            for j, exponent in enumerate(exponents)
        )

        # Биты combination — номера исходных соотношений.
        combination = 1 << index

        while parity:
            pivot = parity.bit_length() - 1

            if pivot not in self.pivots:
                self.pivots[pivot] = (parity, combination)
                return None

            old_parity, old_combination = self.pivots[pivot]
            parity ^= old_parity
            combination ^= old_combination

        # Получена нулевая строка — найдена зависимость.
        self.dependencies += 1

        sums = [0] * len(self.base)
        s = 1

        while combination:
            bit = combination & -combination
            row_index = bit.bit_length() - 1
            left, _, exponents = self.rows[row_index]

            s = s * left % self.n
            sums = [
                total + exponent
                for total, exponent in zip(sums, exponents)
            ]

            combination ^= bit

        assert all(exponent % 2 == 0 for exponent in sums)

        t = 1
        for p, exponent in zip(self.base, sums):
            t = t * pow(p, exponent // 2, self.n) % self.n

        assert (s * s - t * t) % self.n == 0

        for difference in (s - t, s + t):
            divisor = math.gcd(difference, self.n)

            if 1 < divisor < self.n:
                return divisor, s, t

        return None


def print_relations(collector):
    """Небольшая выборка соотношений для консоли."""
    print(f"Гладких соотношений: {len(collector.rows)}")
    print(f"Ранг: {len(collector.pivots)}")
    print(f"Проверено зависимостей: {collector.dependencies}")

    for index, (left, value, exponents) in enumerate(
        collector.rows[:5], start=1
    ):
        # Разреженная запись: выводим только ненулевые показатели.
        factors = [
            (p, exponent)
            for p, exponent in zip(collector.base, exponents)
            if exponent != 0
        ]

        print(f"  Соотношение {index}:")
        print(f"    основание квадрата = {left}")
        print(f"    знаковый остаток   = {value}")
        print(f"    остаток mod n      = {value % collector.n}")
        print(f"    (простой, степень) = {factors}")


# ---------- 1. Ро-метод Полларда ----------

def pollard_rho(n, time_limit=TIME_LIMIT):
    divisor = simple_divisor(n)
    if divisor:
        return divisor

    deadline = time.perf_counter() + time_limit
    total_iterations = 0
    attempt = 0

    while time.perf_counter() < deadline:
        attempt += 1

        # При перезапуске меняем отображение и начальную точку.
        c = attempt
        seed = (attempt + 1) % n
        a = b = seed

        first = []
        last = deque(maxlen=5)
        divisor = 1

        print(f"Попытка {attempt}: f(x) = (x² + {c}) mod n")
        print(f"Начальное значение: {seed}")

        while time.perf_counter() < deadline:
            a = (a * a + c) % n
            b = (b * b + c) % n
            b = (b * b + c) % n

            divisor = math.gcd(abs(a - b), n)
            total_iterations += 1

            row = (total_iterations, a, b, divisor)

            if len(first) < 5:
                first.append(row)
            else:
                last.append(row)

            if divisor != 1:
                break

        print("Итерация, a, b, НОД:")
        for row in first:
            print(" ", row)

        if last:
            print("  ...")
            for row in last:
                print(" ", row)

        print(f"Всего итераций: {total_iterations}")

        if 1 < divisor < n:
            return divisor

        if divisor == n:
            print("НОД = n: перезапуск.")

    print("Остановка по времени.")
    return None


# ---------- 2. Метод Полларда p-1 ----------

def pollard_p_minus_1(n, time_limit=TIME_LIMIT):
    divisor = simple_divisor(n)
    if divisor:
        return divisor

    deadline = time.perf_counter() + time_limit
    iterations = 0

    for bound in (20, 100, 500, 2000, 10000, 50000,
                  200000, 1000000):
        if time.perf_counter() >= deadline:
            print("Остановка по времени.")
            return None

        base = primes_up_to(bound)
        powers = []

        for p in base:
            power = p
            exponent = 1

            while power * p <= bound:
                power *= p
                exponent += 1

            powers.append((p, exponent, power))

        print(
            f"B = {bound}; простых в базе: {len(base)}; "
            f"последний: {base[-1]}"
        )
        print(
            "Показатели l > 1:",
            [(p, exponent) for p, exponent, _ in powers
             if exponent > 1]
        )
        print("Для остальных простых базы l = 1.")

        for a in (2, 3, 5):
            if time.perf_counter() >= deadline:
                print("Остановка по времени.")
                return None

            divisor = math.gcd(a, n)

            if 1 < divisor < n:
                return divisor

            if divisor != 1:
                continue

            value = a

            # Последовательно вычисляем a^M mod n.
            for _, _, power in powers:
                if time.perf_counter() >= deadline:
                    print(f"Прогон с a = {a} не завершён.")
                    return None

                value = pow(value, power, n)

            divisor = math.gcd(value - 1, n)
            iterations += 1

            print(
                f"  Прогон {iterations}: a = {a}; "
                f"a^M mod n = {value}; НОД = {divisor}"
            )

            if 1 < divisor < n:
                return divisor

    print("Заданные границы и основания исчерпаны.")
    return None


# ---------- 3. Квадратичное решето ----------

def quadratic_sieve(n, bound=BASE_BOUND, time_limit=TIME_LIMIT):
    divisor = simple_divisor(n)
    if divisor:
        return divisor

    deadline = time.perf_counter() + time_limit
    base, divisor = make_base(n, bound)

    if divisor:
        return divisor

    collector = RelationCollector(n, base)
    m = math.isqrt(n) + 1

    print(f"B = {bound}; размер базы со знаком: {len(base)}")
    print(f"Последний простой базы: {base[-1]}")
    print(f"m = {m}; f(x) = (m + x)² - n")

    # Учебный поиск корней перебором.
    # Он выполняется один раз для каждого простого базы.
    roots = {}

    for p in base[1:]:
        if time.perf_counter() >= deadline:
            print("Лимит времени при подготовке корней.")
            return None

        roots[p] = [
            r for r in range(p)
            if r * r % p == n % p
        ]

    block_size = 2048
    start = 0
    processed = 0
    result = None

    while time.perf_counter() < deadline:
        values = [
            (m + x) ** 2 - n
            for x in range(start, start + block_size)
        ]
        remainders = values.copy()
        completed = True

        # Настоящее просеивание по арифметическим прогрессиям.
        for p in base[1:]:
            if time.perf_counter() >= deadline:
                completed = False
                break

            positions = {(root - m) % p for root in roots[p]}

            for position in positions:
                first = (position - start) % p

                for j in range(first, block_size, p):
                    while remainders[j] % p == 0:
                        remainders[j] //= p

        if not completed:
            break

        processed += block_size

        for j, remainder in enumerate(remainders):
            if time.perf_counter() >= deadline:
                break

            if remainder != 1:
                continue

            exponents = smooth_factorization(values[j], base)
            left = (m + start + j) % n

            result = collector.add(left, values[j], exponents)

            if result:
                break

        if result:
            break

        start += block_size

    if processed:
        print(f"Полностью просеянный интервал x: [0, {processed - 1}]")
    else:
        print("Ни один блок не просеян полностью.")

    print_relations(collector)

    if result:
        divisor, s, t = result
        print(f"s = {s}")
        print(f"t = {t}")
        print(f"НОД(|s-t|, n) = {math.gcd(abs(s-t), n)}")
        print(f"НОД(s+t, n) = {math.gcd(s+t, n)}")
        return divisor

    print("Остановка по времени без найденного делителя.")
    return None


# ---------- 4. Метод непрерывных дробей ----------

def continued_fraction_method(
    n, bound=BASE_BOUND, time_limit=TIME_LIMIT
):
    divisor = simple_divisor(n)
    if divisor:
        return divisor

    deadline = time.perf_counter() + time_limit
    base, divisor = make_base(n, bound)

    if divisor:
        return divisor

    collector = RelationCollector(n, base)

    root = math.isqrt(n)

    # Параметры рекурсии непрерывной дроби для sqrt(n).
    m = 0
    denominator = 1
    a = root

    # P_{-2} = 0, P_{-1} = 1.
    previous2 = 0
    previous1 = 1

    index = 0
    result = None
    examples = []

    print(f"B = {bound}; размер базы со знаком: {len(base)}")
    print(f"Последний простой базы: {base[-1]}")

    while time.perf_counter() < deadline:
        # Числитель подходящей дроби храним по модулю n.
        numerator = (a * previous1 + previous2) % n
        previous2, previous1 = previous1, numerator

        # Следующие параметры непрерывной дроби.
        m = denominator * a - m
        denominator = (n - m * m) // denominator
        a = (root + m) // denominator

        # Точное тождество:
        # P_i² - n Q_i² = (-1)^(i+1) * denominator.
        remainder = -denominator if index % 2 == 0 else denominator

        exponents = smooth_factorization(remainder, base)

        if exponents is not None:
            if len(examples) < 5:
                examples.append((index, numerator, remainder))

            result = collector.add(
                numerator, remainder, exponents
            )

            if result:
                index += 1
                break

        index += 1

    print(f"Обработано подходящих дробей: {index}")
    print("Первые гладкие примеры (индекс, P_i mod n, r_i):")
    for example in examples:
        print(" ", example)

    print_relations(collector)

    if result:
        divisor, s, t = result
        print(f"s = {s}")
        print(f"t = {t}")
        print(f"НОД(|s-t|, n) = {math.gcd(abs(s-t), n)}")
        print(f"НОД(s+t, n) = {math.gcd(s+t, n)}")
        return divisor

    print("Остановка по времени без найденного делителя.")
    return None


# ---------- Запуск всех методов ----------

def main():
    methods = [
        ("Ро-метод Полларда", pollard_rho),
        ("Метод Полларда p-1", pollard_p_minus_1),
        ("Квадратичное решето", quadratic_sieve),
        ("Метод непрерывных дробей", continued_fraction_method),
    ]

    for n in NUMBERS:
        if n < 3:
            raise ValueError("Ожидается целое n >= 3")

        print("\n" + "=" * 80)
        print(f"Исходное число: {n}")

        for name, method in methods:
            print("\n" + "-" * 60)
            print(name, flush=True)

            started = time.perf_counter()
            divisor = method(n)
            elapsed = time.perf_counter() - started

            if divisor is not None:
                assert 1 < divisor < n
                assert n % divisor == 0

                cofactor = n // divisor

                print("РЕЗУЛЬТАТ:")
                print(f"{n} = {divisor} * {cofactor}")
                print("Проверка произведения:", divisor * cofactor == n)
            else:
                print("РЕЗУЛЬТАТ: делитель в данном запуске не найден.")

            print(f"Время: {elapsed:.6f} с", flush=True)


if __name__ == "__main__":
    main()