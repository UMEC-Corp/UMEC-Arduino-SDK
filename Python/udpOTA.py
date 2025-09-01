import socket
import time

# IP адрес сервера, '' означает прослушивание всех доступных интерфейсов
server_ip = ''
server_port = 1234  # Порт, который прослушивается

# IP адрес и порт устройства ESP32
esp_ip = '192.168.31.17'  # Замените на реальный IP адрес вашего ESP32
esp_port = 1234  # Порт, который слушает ESP32

# Создание сокета
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((server_ip, server_port))

print(f"Listening for UDP packets on {server_ip}:{server_port}")

try:
    while True:
        data, addr = sock.recvfrom(1024)  # Буфер в 1024 байта
        print(f"Received message: {data.decode()} from {addr}")

        # Отправка команды "1" обратно на ESP32 каждые 2 секунды
        time.sleep(2)  # Пауза перед отправкой следующей команды
        sock.sendto(b'1', (esp_ip, esp_port))
        print(f"Sent '1' to ESP32 at {esp_ip}:{esp_port}")
except KeyboardInterrupt:
    print("Server is shutting down.")
    sock.close()
