import socket
import re

# Настройка UDP сокета
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', 1234))  # Привязываем сокет к адресу и порту

# Функция для загрузки существующих MAC-адресов из файла
def load_existing_mac_addresses(file_path):
    try:
        with open(file_path, 'r') as file:
            return set(line.strip() for line in file if line.strip())
    except FileNotFoundError:
        return set()

# Создаем или открываем файл для записи MAC-адресов
with open('C:/firmware/python/mac_addresses.txt', 'a+', buffering=1) as file:
    # Загружаем уникальные MAC-адреса, если файл уже существует
    unique_mac_addresses = load_existing_mac_addresses('C:/firmware/python/mac_addresses.txt')

    try:
        # Бесконечный цикл для прослушивания порта
        while True:
            # Получаем данные из сокета
            data, addr = sock.recvfrom(1024)  # Буфер 1024 байта
            data = data.decode().strip()
            
            if data:  # Если данные получены
                print("Получены данные:", data)
                # Ищем MAC-адрес в строке с помощью регулярного выражения
                match = re.search(r'MAC:\s*([0-9A-Fa-f:]+),', data)
                # Если найден MAC-адрес
                if match:
                    # Извлекаем значение MAC-адреса
                    mac_address = match.group(1)
                    # Проверяем, является ли MAC-адрес уникальным
                    if mac_address not in unique_mac_addresses:
                        file.write(mac_address + '\n')  # Записываем в файл
                        file.flush()  # Сохраняем изменения в файле
                        print("Уникальный MAC-адрес записан в файл.")
                        unique_mac_addresses.add(mac_address)  # Добавляем в множество
    except KeyboardInterrupt:
        print("Прервано пользователем.")
    except Exception as e:
        print(f"Ошибка: {e}")

