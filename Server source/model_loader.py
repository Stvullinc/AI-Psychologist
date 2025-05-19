from llama_cpp import Llama # фреймворк для развертывания и использования LLM моделей
import threading # работа с потоками процессора
from config import Config # файл конфигурационных параметров

# класс модели, гарантирует единственный экземпляр модели в сервисе
class ModelSingleton:
    Isinstance = None # приватное поле класса для хранения единственного экземпляра модели
    lock = threading.Lock()

    # переопределение метода создания экземпляра
    # обеспечивает потоковую безопасность
    def __new__(cls):
        with cls.lock:
            # проверка существования экземпляра
            if not cls.Isinstance:
                cls.Isinstance = super().__new__(cls) # создание нового экземпляра
                cls.Isinstance.init_model() # protected инициализация
            return cls.Isinstance

    # инициализация модели с параметрами
    def init_model(self):
        self.model = Llama(
            model_path=Config.modelPath, # путь к файлу pth
            n_ctx=2**14,  # контекстное окно - объем доступого контекста
            n_gpu_layers=Config.n_gpu_layers, # количество работающих GPU-слоев
            n_batch=Config.n_batch, # размер батча для обработки - выборки данных
            n_threads=Config.n_threads, # количество задействованных CPU-потоков
            verbose=False, # отключение детального логгирования (даже включенное не работало)
            stream=False # отключение режима потоковой передачи - для оптимизации
        )
        self.lock = threading.Lock()

model = ModelSingleton().model # инициализация модели
model_lock = ModelSingleton().lock # блокировка потоков для стабильности и безопасности