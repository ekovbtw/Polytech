cmd
python --version
(убедитесь что python установлен, иначе скачайте)
pip install notebook
jupyter notebook
После этого:
1. автоматически откроется браузер
    
2. откроется интерфейс **Jupyter Notebook**
    

Он выглядит примерно как **файловый менеджер в браузере**.
Если не получается и выдает ошибку, что не видит pip :
python -m pip install notebook
python -m notebook
Далее нужно перейти по ссылке, которая будет в cmd :
http://localhost:8888/tree?token=1f55aeacc1eb4637962d6ca16b9024cca89c3ffb786a0e0e (как пример)

можно еще перед установкой обновить: 
python -m pip install --upgrade pip
