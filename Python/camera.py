import requests
from PIL import Image
from io import BytesIO

# Параметры подключения
local_ip = '192.168.31.32'

# URL для получения изображения
url = f"http://{local_ip}/webcapture.jpg?command=snap&channel=1"

try:
    # Запрос снимка
    print(f"Запрос снимка с {url}...")
    response = requests.get(url)

    # Проверка статуса ответа
    if response.status_code == 200:
        image = Image.open(BytesIO(response.content))
        image.show()  # Показывает изображение
    else:
        print("Не удалось получить снимок, статус код:", response.status_code)
except Exception as e:
    print(f"Ошибка: {e}")
