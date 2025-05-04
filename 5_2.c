#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <string.h>
#include <stdbool.h>

// Макрос для проверки ошибок, если функция возвращает -1
#define CHECK(x) \
    do { \
        if ((x) == -1) { \
            perror(#x); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

// Функция, выполняющая роль ведущего игрока (загадывающего число)
void lead(mqd_t send, mqd_t get, const char* procname, int N) {
    // Выводим, что игрок загадал число
    printf("%s: Я загадал число от 1 до %d. Отгадай его!\n", procname, N);

    srand(time(NULL));  // Инициализация генератора случайных чисел
    int number = rand() % N + 1;  // Генерация случайного числа от 1 до N
    int response = 2;  // Изначально ожидаем попытку от другого игрока
    printf("\tЧисло: %d\n", number);  // Для отладки, на самом деле это число не видит другой игрок

    // Отправляем сигнал "готов к игре"
    CHECK(mq_send(send, (const char*)&response, sizeof(response), 0));

    int attempt;  // Переменная для хранения предположения игрока
    int count = 0;  // Счётчик попыток
    clock_t time1 = clock();  // Засекаем время начала игры

    // Пока игрок не отгадает число, продолжаем цикл
    do {
        count++;  // Увеличиваем счётчик попыток

        while(1) {
            // Получаем попытку угадывания числа от другого игрока
            CHECK(mq_receive(get, (char*)&attempt, sizeof(attempt), NULL)); 
            if (attempt > 0) break;  // Если попытка больше нуля, значит пришло валидное число
        }

        // Проверяем, угадал ли игрок
        if (attempt != number) {
            response = 0;  // Неправильный ответ
            printf("%s: Нет, попробуй еще!\n", procname);
        } else {
            response = 1;  // Правильный ответ
            clock_t time2 = clock();  // Засекаем время окончания игры
            double time_c = (double)(time2 - time1) / CLOCKS_PER_SEC * 1000;  // Вычисляем время в миллисекундах
            printf("%s: Да, это верно!\n", procname);
            printf("Попыток: %d\t Время: %.2f мс\n\n", count, time_c);  // Выводим статистику
        }

        // Отправляем ответ обратно (правильно/неправильно)
        CHECK(mq_send(send, (const char*)&response, sizeof(response), 0));
    } while(attempt != number);  // Игра продолжается, пока число не будет угадано
}

// Функция, выполняющая роль угадывающего игрока
void guess(mqd_t send, mqd_t get, const char* procname, int N) {
    sleep(2);  // Небольшая задержка перед началом игры
    int response;  // Переменная для получения ответа от ведущего игрока
    srand(time(NULL));  // Инициализация генератора случайных чисел

    // Ожидаем сигнала, что ведущий загадал число
    while(1) {
        CHECK(mq_receive(get, (char*)&response, sizeof(response), NULL)); 
        if (response == 2) break;  // Если пришел сигнал 2, начинаем угадывать
    }

    // Массив для отслеживания уже предложенных чисел
    int* used = malloc(N * sizeof(int));
    int used_count = 0;  // Количество использованных чисел

    // Пока число не будет угадано, продолжаем угадывать
    while(response != 1) {
        int attempt;  // Переменная для хранения текущей попытки
        int found;  // Флаг, чтобы не угадывать одно и то же число дважды
        
        do {
            attempt = rand() % N + 1;  // Генерация случайного числа от 1 до N
            found = 0;

            // Проверяем, не предлагалось ли уже это число
            for (int i = 0; i < used_count; i++) {
                if (used[i] == attempt) {
                    found = 1;  // Если число уже было, пробуем другое
                    break;
                }
            }

            if (!found) {
                used[used_count++] = attempt;  // Если число не предлагалось, добавляем его в список использованных
                break;
            }
        } while(1);

        // Отправляем предположение ведущему игроку
        printf("%s: Может это %d?\n", procname, attempt);
        CHECK(mq_send(send, (const char*)&attempt, sizeof(attempt), 0));

        // Ожидаем ответ от ведущего игрока
        while(1) {
            CHECK(mq_receive(get, (char*)&response, sizeof(response), NULL)); 
            if (response == 1 || response == 0) break;  // Ожидаем либо правильный, либо неправильный ответ
        }
    }

    free(used);  // Освобождаем память
}

// Основная функция, создающая процессы и управляющая игрой
int main(int argc, char* argv[]) {
    int iterations = 0;  // Количество раундов
    int N = 0;  // Диапазон для загадывания чисел
    
    // Проверяем аргументы командной строки
    if (argc > 1) {
        N = abs(atoi(argv[1]));  // Диапазон чисел (от 1 до N)
    } else {
        N = 3;  // По умолчанию диапазон 3
    }

    if (argc > 2) {
        iterations = abs(atoi(argv[2]));  // Количество итераций (раундов)
    } else {
        iterations = 2;  // По умолчанию два раунда
    }

    // Выводим параметры игры
    printf("Итерации: %d\n", iterations);
    printf("Диапазон: %d\n", N);
    
    bool leads = true;  // Флаг, который определяет, кто в данный момент ведет
    mqd_t a_mq, b_mq;  // Дескрипторы очередей сообщений
    struct mq_attr attr;  // Атрибуты очереди сообщений
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;  // Максимальное количество сообщений в очереди
    attr.mq_msgsize = sizeof(int);  // Размер сообщения (целое число)

    pid_t pid = fork();  // Создаем дочерний процесс
    CHECK(pid);  // Проверяем ошибку при fork()

    // Основной цикл игры
    for (int i = 0; i < iterations; i++) {
        if(pid > 0) {  // Родительский процесс
            a_mq = mq_open("/queuea", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);  // Открываем очередь для отправки сообщений
            b_mq = mq_open("/queueb", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);  // Открываем очередь для получения сообщений
            if (leads)
                lead(a_mq, b_mq, "proc1", N);  // Если родитель ведет, то он загадывает число
            else
                guess(a_mq, b_mq, "proc1", N);  // Если родитель угадывает, то он пытается угадать
        } else {  // Дочерний процесс
            a_mq = mq_open("/queuea", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);  // Открываем очередь для получения сообщений
            b_mq = mq_open("/queueb", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);  // Открываем очередь для отправки сообщений
            if (!leads)
                lead(b_mq, a_mq, "proc2", N);  // Если дочерний процесс ведет, то он загадывает
            else
                guess(b_mq, a_mq, "proc2", N);  // Если дочерний процесс угадывает, то он пытается угадать
        }
        leads = !leads;  // Меняем флаг, чтобы в следующем раунде роль ведущего менялась
        mq_close(a_mq);  // Закрываем очереди сообщений
        mq_close(b_mq);
    }

    // Удаляем очереди сообщений после завершения игры
    mq_unlink("/queuea");
    mq_unlink("/queueb");
    return 0;
}

