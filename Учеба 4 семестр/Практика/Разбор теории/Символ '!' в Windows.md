В механизме Windows COM символ '!' используется как разделитель между частями составного Moniker. То есть строка: 
```
file://server/share/report.rtf!something
```
интерпретируется примерно следующим образом: 
```
File Moniker
↓
report.rtf
+
Item Moniker
↓
something
```
В итоге получается составной Moniker-object. 

