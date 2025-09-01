Import("env")
from time import sleep

def open_serial_monitor(source, target, env):
    # Задержка, чтобы убедиться, что загрузка завершена
    sleep(2)
    # Открытие Serial Monitor
    env.Execute("pio device monitor")

# Связываем функцию с процессом загрузки
env.AddPostAction("upload", open_serial_monitor)
