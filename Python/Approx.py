import socket
import numpy as np
from scipy.optimize import curve_fit
import json
import qrcode
from PIL import Image, ImageDraw, ImageFont
import os

# Убедимся, что директория для сохранения существует
save_path = "C:\\firmware\\codes"
os.makedirs(save_path, exist_ok=True)

def hyperbola(x, a, b, c):
    """Гиперболическая функция для аппроксимации данных."""
    return a / (x + b) + c

def fit_hyperbola_and_display(data, mac_address):
    """Фиттинг данных, генерация QR-кода с параметрами и отображение."""
    initial_guess = [6000, 15, 10]
    x_data = np.arange(0, 10 * len(data), 10)
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

# Настройка сокета
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind(('', 1234))
sock.listen(1)
print("Сервер запущен и ожидает данные.")


try:
    while True:
        conn, addr = sock.accept()
        print(f"Получено подключение от {addr}")
        data = conn.recv(1024).decode('utf-8')
        print("Полученные данные:", data)  # Вывод данных для отладки
        if data:
            parts = data.split()
            mac_address = parts[0]
            numbers = list(map(float, parts[1:]))
            if len(numbers) == 10:
                fit_hyperbola_and_display(numbers, mac_address)
            else:
                print("Ошибка: необходимо получить ровно 10 чисел.")
        conn.close()
except KeyboardInterrupt:
    print("Сервер остановлен.")
finally:
    sock.close()
