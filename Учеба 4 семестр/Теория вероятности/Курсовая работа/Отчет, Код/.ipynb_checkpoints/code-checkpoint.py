import pandas as pd
import random
random.seed(42)


# 🔹 Загрузка данных
data = pd.read_csv('var_9_dgamma.csv', header=None)
X = data.iloc[:, 0].values

# 🔹 Сумма элементов
def all_summ(X):
    s = 0
    for x in X:
        s += x
    return s


# 🔹 Выборочное среднее
def viborochnoe_avarage(X):
    return all_summ(X) / len(X)


# 🔹 Пузырьковая сортировка
def bubble_sort(X):
    n = len(X)
    X_sorted = list(X)

    for i in range(n):
        for j in range(0, n - i - 1):
            if X_sorted[j] > X_sorted[j + 1]:
                X_sorted[j], X_sorted[j + 1] = X_sorted[j + 1], X_sorted[j]

    return X_sorted


# 🔹 Медиана
def median(X):
    X_sorted = bubble_sort(X)
    n = len(X)

    if n % 2 == 1:
        return X_sorted[n // 2]
    else:
        return (X_sorted[n // 2 - 1] + X_sorted[n // 2]) / 2


# 🔹 Мода
def mode(X):
    counts = {}

    for x in X:
        if x in counts:
            counts[x] += 1
        else:
            counts[x] = 1

    max_count = 0
    mode_value = None

    for key in counts:
        if counts[key] > max_count:
            max_count = counts[key]
            mode_value = key

    return mode_value


# 🔹 Размах
def sample_range(X):
    return max(X) - min(X)


# 🔹 Смещённая дисперсия
def biased_variance(X):
    mean = viborochnoe_avarage(X)
    s = 0

    for x in X:
        s += (x - mean) ** 2

    return s / len(X)


# 🔹 Несмещённая дисперсия
def unbiased_variance(X):
    mean = viborochnoe_avarage(X)
    s = 0

    for x in X:
        s += (x - mean) ** 2

    return s / (len(X) - 1)


# 🔹 Начальный момент k-го порядка
def raw_moment(X, k):
    s = 0

    for x in X:
        s += x ** k

    return s / len(X)


# 🔹 Центральный момент k-го порядка
def central_moment(X, k):
    mean = viborochnoe_avarage(X)
    s = 0

    for x in X:
        s += (x - mean) ** k

    return s / len(X)

# 🔹 Эмперическая функция распределения
def empirical_cdf_value(X, x):
    n = len(X)
    count = 0
    
    for value in X:
        if value <= x:
            count += 1
    
    return count / n


def save_empirical_cdf_to_csv(X, filename):
    # 🔹 Сортируем
    X_sorted = bubble_sort(X)
    n = len(X_sorted)

    # 🔹 Запись в файл
    with open(filename, 'w') as f:
        f.write("x;F(x)\n")

        for i in range(n):
            x = X_sorted[i]
            F_prev = i / n
            F_curr = (i + 1) / n

            # 🔹 горизонтальный участок
            f.write(f"{x};{F_prev}\n")

            # 🔹 скачок вверх
            f.write(f"{x};{F_curr}\n")

# функция для построения гистограммы 
def histogram(X,filename,n):
    X_sorted = bubble_sort(X)
    for i in range(len(X)):
        X_sorted[i] = float(X_sorted[i])    
    kolvo = 20 # тк в доках написал что делим на 20
    danie = [] # в данные будут записаны данные для гистограммы
    # таким образом: danie[i] = [bin_st,bin_fin,count]
    bin_len = (X_sorted[-1]-X_sorted[0])/kolvo
    k=0
    for i in range(kolvo):
        count = 0
        while  k <len(X_sorted) and X_sorted[k] <=float(X_sorted[0])+bin_len*(i + 1) :
            count += 1
            k += 1
        danie.append([float(X_sorted[0])+bin_len*i, float(X_sorted[0])+bin_len*(i + 1),count])
    danie[-1][1] = X_sorted[-1]
    danie1 = []
    for i in range(kolvo):
        danie1.append([round(danie[i][0],2),round(danie[i][2]/n,2)])
        danie1.append([round(danie[i][1],2),round(danie[i][2]/n,2)])

    df = pd.DataFrame(danie1)
    df.to_excel(filename, index=False, header=False) # для этой штуки надо pip install openpyxl

histogram(X,"hist.xlsx",300)
x1 = float(input("Задайте значение x для эмпирической функции = "))
# 🔹 Вычисление и вывод статистик
print("Количество элементов:", len(X))
print("Сумма:", all_summ(X))
print("Выборочное среднее:", viborochnoe_avarage(X))
print("Медиана:", median(X))
print("Мода:", mode(X))
print("Размах:", sample_range(X))
print("Смещённая дисперсия:", biased_variance(X))
print("Несмещённая дисперсия:", unbiased_variance(X))
print("Эмперическая функция F(x) =", empirical_cdf_value(X, x1))


k = int(input("Задайте k для начального и центрального момента = "))
print(f"Начальный момент {k}-го порядка:", raw_moment(X, k))
print(f"Центральный момент {k}-го порядка:", central_moment(X, k))



# 🔹Группы 10 100 200 из выборки
sample_10 = random.sample(list(X), 10)
sample_100 = random.sample(list(X), 100)
sample_200 = random.sample(list(X), 200)

# 🔹 Запись в 3 файла
with open('sample_10.csv', 'w') as f:
    for x in sample_10:
        f.write(f"{x}\n")

with open('sample_100.csv', 'w') as f:
    for x in sample_100:
        f.write(f"{x}\n")

with open('sample_200.csv', 'w') as f:
    for x in sample_200:
        f.write(f"{x}\n")

histogram(sample_10,"hist10.xlsx",10)
histogram(sample_100,"hist100.xlsx",100)
histogram(sample_200,"hist200.xlsx",200)


data = pd.read_csv('sample_10.csv', header=None)
X10 = data.iloc[:, 0].values
x1 = float(input("Задайте значение x для эмпирической функции 10 случайных значений = "))
print("Эмперическая функция F(x) для выборки из 10 случайных значений =", empirical_cdf_value(X10, x1))


data = pd.read_csv('sample_100.csv', header=None)
X100 = data.iloc[:, 0].values
x1 = float(input("Задайте значение x для эмпирической функции 100 случайных значений = "))
print("Эмперическая функция F(x) для выборки из 100 случайных значений =", empirical_cdf_value(X100, x1))

data = pd.read_csv('sample_200.csv', header=None)
X200 = data.iloc[:, 0].values
x1 = float(input("Задайте значение x для эмпирической функции 200 случайных значений = "))
print("Эмперическая функция F(x) для выборки из 200 случайных значений =", empirical_cdf_value(X200, x1))




save_empirical_cdf_to_csv(X10, 'cdf_10.csv')
save_empirical_cdf_to_csv(X100, 'cdf_100.csv')
save_empirical_cdf_to_csv(X200, 'cdf_200.csv')
