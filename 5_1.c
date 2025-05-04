#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/time.h>

// Глобальные переменные, доступные обработчикам сигналов
volatile sig_atomic_t guessed_number = 0;            // Число, которое должен угадать второй игрок
volatile sig_atomic_t attempts = 0;                  // Счётчик количества попыток
volatile sig_atomic_t game_over = 0;                 // Флаг завершения текущего раунда
volatile sig_atomic_t current_guess = 0;             // Текущая догадка угадывающего
volatile sig_atomic_t response_received = 0;         // Флаг получения ответа от "загадывающего" игрока

pid_t other_pid;                                    // PID другого процесса (игрока)

// Обработчик сигнала SIGRTMIN — получает число от угадывающего игрока
void handle_guess(int sig, siginfo_t *info, void *context) {
    if (sig == SIGRTMIN) {
        current_guess = info->si_value.sival_int;    // Получаем число, которое попытался угадать второй игрок
        attempts++;                                   // Увеличиваем количество попыток

        // Если угадываемое число совпало с загаданным числом
        if (current_guess == guessed_number) {
            union sigval value;
            value.sival_int = 1; // Отправляем сигнал, что угадали 
            sigqueue(other_pid, SIGUSR1, value); // Отправляем сигнал первому игроку 
            game_over = 1; // Завершаем игру
        } else {
            union sigval value;
            value.sival_int = 0; // Отправляем сигнал, что догадка неверная
            sigqueue(other_pid, SIGUSR2, value); // Отправляем сигнал первому игроку 
        }
    }
}

// Обработчик ответа от "загадывающего": SIGUSR1 — угадал, SIGUSR2 — не угадал
void handle_response(int sig, siginfo_t *info, void *context) {
    response_received = 1; // Получили ответ от "загадывающего" игрока

    // Если первый игрок (загадывающий) отправил сигнал, что догадка была правильной
    if (sig == SIGUSR1) {
        printf("Угадал! Число было угадано за %d попыток.\n", attempts);
        game_over = 1; // Завершаем игру
    } 
    // Если первый игрок (загадывающий) отправил сигнал, что догадка была неправильной
    else if (sig == SIGUSR2) {
        printf("Неверная попытка (%d). Пробуем снова.\n", current_guess);
    }
}

// Обработчик завершения процесса (например, от родителя к потомку)
void handle_termination(int sig) {
    printf("Получен сигнал завершения. Выход из программы.\n");
    exit(0); // Завершаем процесс
}

// Установка всех необходимых обработчиков 
void setup_signal_handlers() {
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO; // Указываем, что обработчик будет использовать структуру siginfo
    sigemptyset(&sa.sa_mask); // Очистка

    sa.sa_sigaction = handle_guess; 
    sigaction(SIGRTMIN, &sa, NULL); // Обработка сигнала SIGRTMIN (получение числа от угадывающего)

    sa.sa_sigaction = handle_response;
    sigaction(SIGUSR1, &sa, NULL);  // Обработка сигнала, что угадано верно
    sigaction(SIGUSR2, &sa, NULL);  // Обработка сигнала, что неверно угадано

    signal(SIGTERM, handle_termination); // Обработка сигнала завершения (SIGTERM)
}

// Функция угадывания числа — это функция второго игрока
void play_as_guesser(int max_number) {
    int guess; // Переменная для текущей догадки
    srand(time(NULL) ^ getpid()); // Инициализация генератора случайных чисел с учётом PID процесса

    while (!game_over) {
        // Генерируем случайное число от 1 до max_number
        guess = rand() % max_number + 1;
        printf("Пробуем число: %d...\n", guess); // Вывод попытки

        union sigval value;
        value.sival_int = guess; // Установка догадки в значение сигнала

        response_received = 0; // Сброс флаг получения ответа
        sigqueue(other_pid, SIGRTMIN, value); // Отправка попытки первому игроку 

        while (!response_received) {
            pause(); // Ждём ответа на нашу догадку
        }
    }
}

// Функция загадывания числа — это функция первого игрока
void play_as_chooser(int max_number) {
    srand(time(NULL) ^ getpid()); // Инициализация генератора случайных чисел с учётом PID процесса
    guessed_number = rand() % max_number + 1; // Генерация числа от 1 до max_number

    printf("Я загадал число от 1 до %d. Жду, когда его угадают...\n", max_number);

    while (!game_over) {
        pause(); // Ожидание попытки от второго игрока (угадывающего)
    }

    printf("Число было: %d. Раунд завершён.\n", guessed_number); // Выводим результат после завершения раунда
}

// Вычисление разницы времени в миллисекундах
long get_time_diff_ms(struct timeval start, struct timeval end) {
    // Вычисление разницы во времени между двумя моментами
    return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <максимальное_число>\n", argv[0]);
        return 1;
    }

    // Получение максимального числа от пользователя
    int max_number = atoi(argv[1]);
    if (max_number <= 1) {
        fprintf(stderr, "Максимальное число должно быть больше 1.\n");
        return 1;
    }

    pid_t pid = fork(); // Создание процесса (второго игрока)

    if (pid == -1) {
        perror("Ошибка при создании процесса");
        return 1;
    }

    pid_t self_pid = getpid();

    if (pid == 0) { // Потомок (второй игрок)
        setup_signal_handlers(); // Настройка обработчика сигналов
        other_pid = getppid(); 

        for (int i = 0; i < 10; i++) { 
            game_over = 0; // Обнуляем флаг завершения игры
            attempts = 0; // Обнуляем счётчик попыток

            struct timeval start, end;
            gettimeofday(&start, NULL); // Запоминаем время начала раунда

            printf("\n[Раунд %d] ", i + 1);
            if (i % 2 == 1) {
                printf("Потомок загадывает.\n");
                play_as_chooser(max_number); // Если нечётный раунд — потомок загадывает число
            } else {
                printf("Потомок угадывает.\n");
                play_as_guesser(max_number); // Если чётный раунд — потомок угадывает число
            }

            gettimeofday(&end, NULL); // Запоминаем время конца раунда
            long duration = get_time_diff_ms(start, end); // Вычисляем продолжительность раунда
            printf("[Раунд %d] Время: %ld миллисекунд\n", i + 1, duration);
        }

        exit(0); // Завершаем процесс потомка
    } else { // Родитель (первый игрок)
        setup_signal_handlers(); // Настроим обработчики сигналов
        other_pid = pid; // Сохраняем PID потомка

        for (int i = 0; i < 10; i++) { // Играть 10 раундов
            game_over = 0; // Обнуляем флаг завершения игры
            attempts = 0; // Обнуляем счётчик попыток

            struct timeval start, end;
            gettimeofday(&start, NULL); // Запоминаем время начала раунда

            printf("\n[Раунд %d] ", i + 1);
            if (i % 2 == 0) {
                printf("Родитель загадывает.\n");
                play_as_chooser(max_number); // Если чётный раунд — родитель загадывает число
            } else {
                printf("Родитель угадывает.\n");
                play_as_guesser(max_number); // Если нечётный раунд — родитель угадывает число
            }

            gettimeofday(&end, NULL); // Запоминаем время конца раунда
            long duration = get_time_diff_ms(start, end); // Вычисляем продолжительность раунда
            printf("[Раунд %d] Время: %ld миллисекунд\n", i + 1, duration);
        }

        // Отправляем сигнал завершения потомку и ждём его завершения
        kill(pid, SIGTERM); 
        waitpid(pid, NULL, 0);

        printf("Игра завершена. Все раунды сыграны.\n");
    }

    return 0;
}

