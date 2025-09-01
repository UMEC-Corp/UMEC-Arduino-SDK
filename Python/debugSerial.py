import serial
from datetime import datetime

# Открываем порт COM4
ser = serial.Serial('COM3', 115200)

# Инициализируем счетчики
reboot_count = 0
sensor_count = 0
sensor_wifi_count = 0

try:
    while True:
        try:
            # Читаем строку из порта COM4 с игнорированием недекодируемых символов
            line = ser.readline().decode('utf-8', errors='ignore').strip()
        except UnicodeDecodeError:
            # Пропускаем строки, которые не могут быть декодированы
            continue
        
        # Получаем текущее время
        current_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        # Проверяем, содержится ли искомое число в строке
        if "VERSION FIRMWARE" in line:
            # Увеличиваем счетчик перезагрузок
            reboot_count += 1
            print(f"{current_time} - Количество перезагрузок: {reboot_count}") 
        
        # Проверяем, содержится ли фраза "Датчик t11" в строке
        if "MQTT reconnected successfully" in line:
            # Увеличиваем счетчик обработанных сообщений
            sensor_count += 1
            print(f"{current_time} - Переподключений к MQTT: {sensor_count}")

        # Проверяем, содержится ли фраза "Publish" в строке
        if "Publish" in line:
            # Увеличиваем счетчик обработанных сообщений
            sensor_wifi_count  += 1
            print(f"{current_time} - Обработано сообщений MQTT: {sensor_wifi_count}")

            # Проверяем, содержится ли фраза "Датчик t11" в строке
        if "SSL" in line:
            # Увеличиваем счетчик обработанных сообщений
            sensor_count += 1
            print(f"{current_time} - Обработано ошибок SSL: {sensor_count}")
            
except KeyboardInterrupt:
    # Обработка прерывания с клавиатуры (Ctrl+C)
    print("Программа завершена.")
    ser.close()
