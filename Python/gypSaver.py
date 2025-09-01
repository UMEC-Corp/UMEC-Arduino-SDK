import socket
import numpy as np
from scipy.optimize import curve_fit
import json
import qrcode
from PIL import Image, ImageDraw, ImageFont
import os
import re

# Убедимся, что директория для сохранения существует
save_path = "C:\\firmware\\codes"
os.makedirs(save_path, exist_ok=True)

def hyperbola(x, a, b, c):
    """Гиперболическая функция для аппроксимации данных."""
    return a / (x + b) + c

def fit_hyperbola_and_display(data, mac_address, send_socket):
    """Фиттинг данных, генерация QR-кода с параметрами и отображение."""
    initial_guess = [6000, 15, 10]
    x_data = np.arange(0, 10 * len(data), 10) # Генерируем массив x-значений
#    x_data = np.array([0, 30, 60, 90])  # Заданные значения x
    try:
        popt, _ = curve_fit(hyperbola, x_data, data, p0=initial_guess)
        result = {"m": 100, "a": round(popt[0], 2), "b": round(popt[1], 2), "c": round(popt[2], 2)}
        json_result = json.dumps(result)
        print(json_result)

        qr = qrcode.make(json_result)
        mac_address_filename = mac_address.replace(':', '_')
        qr_file_name = os.path.join(save_path, f"{mac_address_filename}.png")
        qr.save(qr_file_name)

        image = Image.open(qr_file_name)
        img_w, img_h = image.size
        bg = Image.new("RGB", (img_w, img_h + 100), "white")
        bg.paste(image, (0, 0))
        draw = ImageDraw.Draw(bg)
        try:
            font = ImageFont.truetype("arialbd.ttf", 24)
        except IOError:
            font = ImageFont.load_default()
        text = f"MAC: {mac_address}\na: {result['a']}, b: {result['b']}, c: {result['c']}"
        text_width, text_height = draw.textbbox((0, 0), text, font=font)[2:]
        draw.text(((img_w - text_width) // 2, img_h + 10), text, font=font, fill="black")
        bg.save(qr_file_name)
        bg.show()

        # Отправляем JSON результат на специфичный адрес и порт
        send_socket.sendto(json_result.encode(), ('192.168.1.184', 4321))
    except Exception as e:
        error_message = f"Error: {str(e)}"
        print(error_message)
        send_socket.sendto(error_message.encode(), ('192.168.1.184', 4321))

# Создаем UDP-сокет для приема данных
recv_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
recv_sock.bind(('', 1234))

# Создаем UDP-сокет для отправки сообщений
send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print("UDP сервер запущен и ожидает данные.")

try:
    while True:
        data, address = recv_sock.recvfrom(4096)
        print("Получены данные:", data.decode().strip())
        data = data.decode().strip()
        match = re.search(r'(\S+) ((?:\d+ )+\d+)$', data)
        if match:
            mac_address = match.group(1)
            numbers = list(map(float, match.group(2).split()))
            if len(numbers) == 10: # Проверяем, что получено 10 чисел
#           if len(numbers) == 4:  # Проверяем, что получено 4 числа
                fit_hyperbola_and_display(numbers, mac_address, send_sock)
            else:
                error_message = f"Error: Incorrect number of values, received {len(numbers)} instead of 10."
                print(error_message)
                send_sock.sendto(error_message.encode(), ('192.168.1.184', 4321))
except Exception as e:
    print("Произошла ошибка:", e)
finally:
    recv_sock.close()
    send_sock.close()
