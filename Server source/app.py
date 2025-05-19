# блок импорта необходимых библиотек
from flask import (
    Flask, # фреймворк для хостинга
    request, # обработка HTTP запросов
    jsonify # преобразование ответов в JSON
)
from flask_limiter import Limiter # ограничение количества запросов - защита от возможных атак
from flask_limiter.util import get_remote_address # запись IP
from model_loader import model, model_lock # импорт объектов развёрнутой модели и объекта блокировки потоков
from session_manager import session_manager # менеджер сессий
import uuid # генерация токенов для доступа к диалогу
from config import Config # файл конфигурационных параметров
import logging #модуль логгирования событий
from logging.handlers import RotatingFileHandler #ротация лог-файлов (для оптимизации размеров файла логов)
import json # работа с JSON
import re # для конвертации md -> HTML


USERS_FILE = Config.USERS_FILE # путь к базе данных пользователей

# запуск сервиса
app = Flask(__name__) # инициализация хостинг-сервер
# лимит запросов к серверу
limiter = Limiter(
    app=app,
    key_func=get_remote_address,
    storage_uri="memory://",
    default_limits=["200 per hour",]
)
# инициализация логгера
logger = logging.getLogger('my_app')
logger.setLevel(logging.INFO)

# обработчик для всех логов
handler = RotatingFileHandler(
    'app.log',
    maxBytes=1000000, # максимальный размер файла
    backupCount=3,
    encoding='utf-8'
)
# установка формата вывода логов
handler.setFormatter(
    logging.Formatter
        (
        '%(asctime)s - %(levelname)s - %(message)s'
    )
)
logger.addHandler(handler) # подключение к обработчику событий


"""
не работает
llama_logger = logging.getLogger('llama.cpp')
llama_logger.propagate = False
llama_logger.addHandler(handler)
llama_logger.setLevel(logging.INFO)
"""


# конвертация ответа модели в HTML формат
def md_to_html(text):
    # обработка заголовков
    text = re.sub(r'^#\s+(.*?)\s*$', r'<h1>\1</h1>', text, flags=re.MULTILINE)
    text = re.sub(r'^##\s+(.*?)\s*$', r'<h2>\1</h2>', text, flags=re.MULTILINE)
    # обработка жирного текста и курсива
    text = re.sub(r'\*\*(.*?)\*\*', r'<b>\1</b>', text)
    text = re.sub(r'\*(.*?)\*', r'<i>\1</i>', text)
    # обработка нумерованных/маркированных списков
    lines = text.split('\n')
    in_list = False
    list_type = None
    result = []
    for line in lines:
        line = line.strip()
        if not line:
            continue
        # нумерованные списки
        if re.match(r'^\d+\.\s+', line):
            if not in_list or list_type != 'ol':
                if in_list:
                    result.append(f'</{list_type}>')
                result.append('<ol>')
                list_type = 'ol'
                in_list = True
            item = re.sub(r'^\d+\.\s+(.*)', r'<li>\1</li>', line)
            result.append(item)
        # маркированные списки
        elif re.match(r'^-\s+', line):
            if not in_list or list_type != 'ul':
                if in_list:
                    result.append(f'</{list_type}>')
                result.append('<ul>')
                list_type = 'ul'
                in_list = True
            item = re.sub(r'^-\s+(.*)', r'<li>\1</li>', line)
            result.append(item)
        elif in_list:
            result.append(f'</{list_type}>')
            in_list = False
            list_type = None
            result.append(line)
        else:
            result.append(line) # текст без тегов
    if in_list:
        result.append(f'</{list_type}>')
    text = '\n'.join(result)
    paragraphs = re.split(r'\n{2,}', text)
    processed = []
    for p in paragraphs:
        if not p.strip():
            continue
        if re.match(r'^\s*<[a-z]+>', p) or re.match(r'^\s*</[a-z]+>', p):
            processed.append(p)
        else:
            p = p.replace('\n', '<br>')
            processed.append(f'<p>{p}</p>')
    text = '\n'.join(processed)
    text = text.replace('\n', "<br>").replace("><br><", "><")
    return text

# функция загрузки из БД
def load_users():
    with open(USERS_FILE, 'r') as f:
        return json.load(f)

# функция сохранения БД с изменениями
def save_users(users):
    with open(USERS_FILE, 'w') as f:
        json.dump(users, f, indent=4)

# функция поиска пользователя в БД по токену
def find_user_by_token(token):
    users = load_users()
    for username, data in users.items():
        if data.get('token') == token:
            return username
    return None

# метод регистрации
@app.route('/register', methods=['POST']) # обработчик запроса, метод POST - исходящий запрос
@limiter.limit("20 per minute") # ограничение 20 запросов в минуту
def register():
    try:
        # получение запроса от пользователя
        data = request.json # чтение в JSON формате
        user_ip = request.remote_addr # получение IP пользователя
        users = load_users() # открытие БД
        username = next(iter(data.keys())) if data else None # получение первого ключа из JSON как логина
        # проверка валидности запроса
        if not username: # если запрос поврежден - выводим ошибку
            logger.warning(f"[ОШИБКА РЕГИСТРАЦИИ] - пустой username: IP - {user_ip}")
            return jsonify({"error": "Username required"}), 400 # возврат кода ошибки: 400 - bad request - неверный запрос
        # если пользователь уже существует -> возврат ошибки + логгирование
        if username in users:
            logger.warning(
                f"[ОШИБКА РЕГИСТРАЦИИ] - пользователь уже существует: IP - {user_ip} ###### Username - {username}")
            return jsonify({"error": "вы уже зарегистрированы"}), 400

        # получение пароля
        password = data[username].get('password')

        if not password: # возврат ошибки если запрос некорректен
            logger.warning(f"[ОШИБКА РЕГИСТРАЦИИ] - пароль не указан: IP - {user_ip} ###### Username - {username}")
            return jsonify({"error": "Password required"}), 400

        # генерация токена для доступа к запросам
        user_token = str(uuid.uuid4())

        # добавление пользователя в БД
        users[username] = {
            "password": password,
            "token": user_token
        }
        save_users(users)

        # инициализация начальной истории с системным промптом
        initial_history = [{
            "role": Config.role_list[0], # system
            "content": Config.system_prompt # начальный промпт
        }]

        # сохранение истории
        session_manager.save_history(username, initial_history)
        logger.info(f"[РЕГИСТРАЦИЯ УСПЕШНА]: IP - {user_ip} ###### Username - {username}") # логгирование
        return jsonify({
            "success": True,
            "token": user_token,
            "history": initial_history  # для загрузки истории на клиентской части
        })
    except Exception as e:
        logger.error(f"[ОШИБКА РЕГИСТРАЦИИ]: {str(e)}")
        return jsonify({"error": "Internal server error"}), 500

# функция регистрации
@app.route('/login', methods=['POST'])
@limiter.limit("20 per minute")
def login():
    try:
        # получение запроса и IP
        data = request.json
        user_ip = request.remote_addr

        # проверка запроса
        if 'username' not in data or 'password' not in data:
            logger.warning(f"[ОШИБКА ВХОДА] - неверный формат запроса: IP - {user_ip}")
            return jsonify({"error": "Неверный формат запроса"}), 400

        # парсинг JSON
        username = data['username']
        password = data['password']
        users = load_users()

        # проверка валидности запроса
        if username not in users:
            logger.warning(f"[ОШИБКА ВХОДА] - пользователя не существует: IP - {user_ip}")
            return jsonify({"error": "Пользователь не найден"}), 400
        user = users[username]
        if user['password'] != password:
            logger.warning(f"[ОШИБКА ВХОДА] - неверный пароль: IP - {user_ip} ###### Username - {username}")
            return jsonify({"error": "Неверный пароль"}), 401
        logger.info(f"[УСПЕШНЫЙ ВХОД]: IP - {user_ip} ###### Username - {username}")
        # загрузка истории
        user_history = session_manager.load_history(username)
        # форматирование истории для конвертации в JSON
        formatted_history = [
            {"role": msg["role"], "content": msg["content"]}
            for msg in user_history
        ]
        # преобразование к формату JSON
        result = {
            "success": True,
            "token": user['token'],
            "history": formatted_history
        }
        # print(res)
        return jsonify(result) # ответный запрос

    except Exception as e:
        logger.error(f"[ОШИБКА ВХОДА]: {str(e)}")
        return jsonify({"error": "Внутренняя ошибка сервера"}), 500

# обработчик превышения запросов
@app.errorhandler(429)
def login_limit_handler(e):
    user_ip = request.remote_addr
    logger.warning(f"[ПРЕВЫШЕНИЕ ЛИМИТА] - IP: {user_ip}")
    return jsonify({"error": str(e)}), 429

# функция общения с ИИ
@app.route('/chat', methods=['POST'])
@limiter.limit("10 per minute")
def chat():
    try:
        data = request.get_json()
        user_token = data.get('token')
        username = find_user_by_token(user_token)
        if not username:
            logger.warning(f"[ОШИБКА] Неверный токен: {user_token}")
            return jsonify({"error": "Неверный токен"}), 401
        history = session_manager.load_history(username)
        # print(data)
        user_input = data['question'] # получение текста пользовательского запроса
        # добавление в историю
        history.append({
            "role": Config.role_list[1],  # user
            "content": user_input
        })

        # ГЕНЕРАЦИЯ ОТВЕТА
        with model_lock:
            response = model.create_chat_completion(
                messages=history,
                **Config.MODEL_PARAMS # загрузка параметров модели
            )
        ai_response = response['choices'][0]['message']['content'] # добавение в историю
        ai_response = md_to_html(ai_response) # конвертация md -> HTML
        history.append(
            {
                "role": Config.role_list[2],  # assistant
                "content": ai_response
            }
        )
        # сохранение истории
        session_manager.save_history(username, history)
        print(ai_response) # для дебага
        # возврат ответа
        logger.info(
            f"[ОТВЕТ]: IP - {request.remote_addr}  ###### текст - {ai_response}"
        )
        # ответ пользователю
        return jsonify({
            "answer": ai_response,
            "status": "success",
            "history": history
        })
    except Exception as e:
        print(e)
        logger.error(
            f"[ОШИБКА]: IP - {request.remote_addr}  ###### текст ошибки - {str(e)}"
        )
        print(e)
        return jsonify({"error": str(e)}), 500


'''
это неудачный эксперимент

if __name__ == '__main__':
    app.run(
        host=Config.SERVER['host'],
        port=Config.SERVER['port'],
        debug=Config.SERVER['debug']
    )
'''

# хостинг сервиса на внешнем IP
if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=True, port=8642)

    '''
    пробовали другой фреймворк
    
    console_logging = logging.StreamHandler()
    console_logging.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
    console_logging.addHandler(handler)
    '''

    #serve(app, host='0.0.0.0', port=8642) # и это тоже