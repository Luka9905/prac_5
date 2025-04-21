#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>

volatile int secret_number;
volatile int attempts = 0;
volatile pid_t guessing_pid;
volatile int low = 1;
volatile int high;
volatile int total_rounds = 10;
volatile int current_round = 0;
volatile sig_atomic_t guess_received = 0;
volatile sig_atomic_t current_guess = 0;
volatile sig_atomic_t round_finished = 0;

struct timeval start_time, end_time;

void print_time_stats() {
    gettimeofday(&end_time, NULL);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long microseconds = end_time.tv_usec - start_time.tv_usec;
    double elapsed = seconds + microseconds * 1e-6;
    printf("⏱ Время раунда: %.3f секунд\n", elapsed);
}

void guess_correct(int sig) {
    printf("\n Процесс [%d] угадал число %d за %d попыток!\n", getpid(), secret_number, attempts);
    print_time_stats();
    round_finished = 1;
    exit(0);
}

void guess_incorrect(int sig) {
    printf(" Процесс [%d]: Не угадано!\n", getpid());
}

void handle_guess(int sig, siginfo_t *info, void *context) {
    current_guess = info->si_value.sival_int;
    guess_received = 1;
}

void send_guess(int guess) {
    union sigval value;
    value.sival_int = guess;
    if (sigqueue(guessing_pid, SIGRTMIN, value)) {
        perror("Ошибка при отправке предположения");
        exit(1);
    }
}

void make_guess() {
    if (low > high || round_finished) {
        return;
    }

    int guess = low + (high - low) / 2;
    attempts++;
    printf("Процесс [%d] пытается угадать: %d\n", getpid(), guess);
    send_guess(guess);

    if (guess == secret_number) {
        kill(guessing_pid, SIGUSR1);
        round_finished = 1;
    } else {
        if (guess < secret_number) {
            printf("➡ Процесс [%d]: Нужно больше!\n", getpid());
            low = guess + 1;
        } else {
            printf("⬅ Процесс [%d]: Нужно меньше!\n", getpid());
            high = guess - 1;
        }
        kill(guessing_pid, SIGUSR2);
    }
}

void play_guesser() {
    printf("\n Процесс [%d] начал угадывать число\n", getpid());
    
    signal(SIGUSR1, guess_correct);
    signal(SIGUSR2, guess_incorrect);
    
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_guess;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);

    while (!round_finished) {
        pause();
        if (guess_received) {
            guess_received = 0;
        }
    }
    exit(0);
}

void play_hider(int N) {
    secret_number = rand() % N + 1;
    attempts = 0;
    low = 1;
    high = N;
    round_finished = 0;
    
    printf("\n=== Раунд %d ===\n", current_round);
    printf(" Процесс [%d] загадал число от 1 до %d\n", getpid(), N);
    gettimeofday(&start_time, NULL);
    
    signal(SIGALRM, make_guess);
    
    while (!round_finished && attempts < 10) {
        alarm(1);
        pause();
    }

    if (!round_finished) {
        printf("\n Процесс [%d] не угадал число %d за 10 попыток!\n", guessing_pid, secret_number);
        print_time_stats();
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <N>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    high = N;
    srand(time(NULL));

    printf(" Главный процесс [%d] запустил игру\n", getpid());

    while (current_round < total_rounds) {
        current_round++;
        
        // Игрок 1 загадывает, игрок 2 угадывает
        pid_t player2 = fork();
        if (player2 < 0) {
            perror("Ошибка fork");
            exit(1);
        }

        if (player2 == 0) {
            play_guesser();
        } else {
            guessing_pid = player2;
            play_hider(N);
            
            kill(player2, SIGTERM);
            wait(NULL);
            
            // Меняемся ролями
            pid_t player1 = fork();
            if (player1 < 0) {
                perror("Ошибка fork");
                exit(1);
            }

            if (player1 == 0) {
                play_guesser();
            } else {
                guessing_pid = player1;
                play_hider(N);
                
                kill(player1, SIGTERM);
                wait(NULL);
            }
        }
    }

    printf("\n Главный процесс [%d] завершил игру. Сыграно %d раундов.\n", getpid(), total_rounds);
    return 0;
}
