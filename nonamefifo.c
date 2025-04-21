#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <wait.h>

int main() {
    int pipe_fd[2]; // Массив для хранения дескрипторов канала
    int secret_number, guess;
    int low, high; // Диапазон для угадывания
    int rounds = 10; // Количество итераций
    int current_round = 0; // Счетчик итераций

    // Ввод правой границы диапазона с клавиатуры
    printf("Введите правую границу диапазона (должна быть больше 1): ");
    scanf("%d", &high);
    
    // Проверка корректности ввода
    if (high <= 1) {
        printf("Ошибка: правая граница должна быть больше 1!\n");
        return EXIT_FAILURE;
    }

    // Создаем неименованный канал
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    while (current_round < rounds) {
        // Код для игрока 1 (родительский процесс)
        if (fork() == 0) {
            // Дочерний процесс (Игрок 1)
            close(pipe_fd[0]); // Закрываем чтение в канале

            // Генерируем случайное число от 1 до high
            srand(time(NULL) + current_round);
            secret_number = rand() % high + 1;

            // Записываем загаданное число в канал
            write(pipe_fd[1], &secret_number, sizeof(secret_number));
            printf("Игрок 1 загадал число от 1 до %d: %d\n", high, secret_number);
            close(pipe_fd[1]);
            exit(EXIT_SUCCESS);
        }

        wait(NULL);

        // Код для игрока 2 (родительский процесс)
        if (fork() == 0) {
            // Второй дочерний процесс (Игрок 2)
            close(pipe_fd[1]); // Закрываем запись в канале

            // Читаем загаданное число от игрока 1
            read(pipe_fd[0], &secret_number, sizeof(secret_number));
            printf("Игрок 1 загадал число. Игрок 2, начинай угадывать!\n");

            low = 1;
            int current_high = high; // Используем введенную правую границу
            int attempts = 0;

            while (1) {
                guess = low + (current_high - low) / 2;
                printf("Игрок 2 пытается угадать: %d\n", guess);
                attempts++;

                if (guess == secret_number) {
                    printf(" Игрок 2 угадал число %d правильно за %d попыток!\n", guess, attempts);
                    break;
                } else if (guess < secret_number) {
                    printf(" Игрок 2 угадал слишком маленькое число.\n");
                    low = guess + 1;
                } else {
                    printf(" Игрок 2 угадал слишком большое число.\n");
                    current_high = guess - 1;
                }
                
                if (low > current_high) {
                    printf(" Игрок 2 не может угадать число. Проверьте диапазон.\n");
                    break;
                }
            }
            
            close(pipe_fd[0]);
            exit(EXIT_SUCCESS);
        }

        wait(NULL);
        current_round++;

        if (current_round >= rounds) break;

        printf("\n--- Раунд %d завершен! Теперь игроки меняются ролями. ---\n", current_round);

        pipe(pipe_fd); // Новый канал для следующего раунда

        if (fork() == 0) {
            close(pipe_fd[0]);
            srand(time(NULL) + current_round);
            secret_number = rand() % high + 1;
            write(pipe_fd[1], &secret_number, sizeof(secret_number));
            printf("Игрок 2 загадал число от 1 до %d: %d\n", high, secret_number);
            close(pipe_fd[1]);
            exit(EXIT_SUCCESS);
        }

        wait(NULL);

        if (fork() == 0) {
            close(pipe_fd[1]);
            read(pipe_fd[0], &secret_number, sizeof(secret_number));
            printf("Игрок 2 загадал число. Игрок 1, начинай угадывать!\n");

            low = 1;
            int current_high = high;
            int attempts = 0;

            while (1) {
                guess = low + (current_high - low) / 2;
                printf("Игрок 1 пытается угадать: %d\n", guess);
                attempts++;

                if (guess == secret_number) {
                    printf(" Игрок 1 угадал число %d правильно за %d попыток!\n", guess, attempts);
                    break;
                } else if (guess < secret_number) {
                    printf(" Игрок 1 угадал слишком маленькое число.\n");
                    low = guess + 1;
                } else {
                    printf(" Игрок 1 угадал слишком большое число.\n");
                    current_high = guess - 1;
                }

                if (low > current_high) {
                    printf(" Игрок 1 не может угадать число. Проверьте диапазон.\n");
                    break;
                }
            }
            
            close(pipe_fd[0]);
            exit(EXIT_SUCCESS);
        }

        wait(NULL);
    }

    printf(" Игра завершена! Игроки сыграли %d раундов в диапазоне 1-%d.\n", rounds, high);
    return EXIT_SUCCESS;
}
