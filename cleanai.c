#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

#include "libs/cJSON.h"
#include "libs/cJSON.c"

#include "libs/miniz.h"
#include "libs/miniz.c"

//Customisation
#define eta_pause_toggle true //Eta will pause program for eta_pause_time_ms for you to see eta
#define eta_pause_time_ms 1000 //Time to pause
#define eta_pause_every_n_batch 3 //pause every n batch
//End Customisation

float lr_plateau_best_loss = __FLT_MAX__;
int lr_plateau_counter = 0;

#ifdef _WIN32 //windows compability is pain ;(
#include <windows.h>
#include <conio.h>
#include <tlhelp32.h>

long long time_ms(){
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (long long)(uli.QuadPart / 10000);
}

char* input_with_timeout(char* qry, int timeout_ms){
    printf("%s", qry);
    fflush(stdout);
    
    char* buff = malloc(4096);
    if (!buff) {
        printf("Failed to allocate memory to read input.\n");
        return NULL;
    }
    
    int pos = 0;
    long long start_time = time_ms();
    
    while (pos < 4095) {
        if (_kbhit()) {
            int k = _getch();
            if (k == 0 || k == 0xE0) { (void)_getch(); continue; }
            char c = (char)k;
            
            if (c == '\r' || c == '\n') {
                putchar('\n');
                buff[pos] = '\0';
                return buff;
            } else if (c == '\b' && pos > 0) {
                printf("\b \b");
                pos--;
            } else if (c >= 32 && c <= 126) {
                putchar(c);
                buff[pos++] = c;
            }
        }
        
        if (time_ms() - start_time >= timeout_ms) {
            free(buff);
            return NULL;
        }
        
        Sleep(1);
    }
    
    buff[pos] = '\0';
    return buff;
}

typedef HANDLE Thread;
typedef DWORD THREAD_RETURN;
#define THREAD_CALL WINAPI

#define thread_start(fn, arg, out_thread)                     \
    do {                                                      \
        out_thread = CreateThread(NULL, 0, fn, arg, 0, NULL); \
    } while (0)

#define thread_join(thread, ret_ptr)                \
    do {                                            \
        WaitForSingleObject(thread, INFINITE);      \
        DWORD code;                                 \
        GetExitCodeThread(thread, &code);           \
        CloseHandle(thread);                        \
        *(void**)ret_ptr = (void*)code;             \
    } while (0)

#else
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

long long time_ms(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
}

char* input_with_timeout(char* qry, int timeout_ms){
    printf("%s", qry);
    fflush(stdout);
    
    char* buff = malloc(4096); //Well here and in the other version its fine cuz this one is used for nothing important so idc
    if (!buff) {
        printf("Failed to allocate memory to read input.\n");
        return NULL;
    }
    
    fd_set readfds;
    struct timeval tv;
    long long start_time = time_ms();
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        long long elapsed = time_ms() - start_time;
        if (elapsed >= timeout_ms) {
            break;
        }
        
        long long remaining = timeout_ms - elapsed;
        tv.tv_sec = remaining / 1000;
        tv.tv_usec = (remaining % 1000) * 1000;
        
        int result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        
        if (result > 0) {
            if (fgets(buff, 4096, stdin)) {
                size_t len = strlen(buff);
                if (len > 0 && buff[len - 1] == '\n') {
                    buff[len - 1] = '\0';
                }
                return buff;
            }
        } else if (result == 0) {
            break;
        } else {
            break;
        }
    }
    
    free(buff);
    return NULL;
}

#include <pthread.h>

typedef pthread_t Thread;
typedef void* THREAD_RETURN;
#define THREAD_CALL

#define thread_start(fn, arg, out_thread)               \
    do {                                                \
        pthread_create(&out_thread, NULL, fn, arg);     \
    } while (0)

#define thread_join(thread, ret_ptr)                \
    do {                                            \
        pthread_join(thread, (void**)ret_ptr);      \
    } while (0)

#endif
int itoa(int value, char* buff, int base){
    //fuck base
    return sprintf(buff, "%d", value);
}

void sleep_ms(unsigned int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

char* input(char* qry){
    printf("%s", qry);
    fflush(stdout);
    char* buff = malloc(512 * 1024); //4kb is sufficient -- future note, I am a dumbass AND NO, so yea 512kb
    if (!buff){
        printf("Failed to allocate memory to read input.\n");
        return NULL;
    }
    if (fgets(buff, 512 * 1024, stdin)){
        size_t len = strlen(buff);
        if (len > 0 && buff[len - 1] == '\n') {
            buff[len - 1] = '\0'; //remove the newline
        }
        return buff;
    }
    else{
        free(buff);
        return NULL;
    }
}

char* input_multiline(char* qry){
    printf("%s", qry);
    size_t qry_len = strlen(qry);
    qry_len -= 2; //Stupid terminal alligment fix
    char spacer[qry_len + 1];
    memset(spacer, ' ', qry_len);
    spacer[qry_len] = '\0';

    char* resp = calloc(1, 1); //Dummy init
    if (!resp){
        printf("Failed to allocate memory to read multiline input.\n");
    }
    size_t resp_len = 0;

    char* buff = malloc(512 * 1024);
    if (!buff){
        printf("Failed to allocate memory to read multiline input.\n");
        free(resp);
        return NULL;
    }

    while (true){
        if (!fgets(buff, 512 * 1024, stdin)){
            printf("\n");
            fflush(stdout);
            break;
        }
        size_t buff_dat_len = strlen(buff);
        resp_len += buff_dat_len;
        char* tmp = realloc(resp, resp_len + 1);
        if (!tmp){
            printf("Failed to allocate memory to read multiline input.\n");
            free(resp);
            free(buff);
            return NULL;
        }
        resp = tmp;

        strcat(resp, buff);
        memset(buff, 0, 512 * 1024);
        
        printf("%s", spacer);
        fflush(stdout);
    }

    clearerr(stdin);
    free(buff);
    return resp;
}

void help(char* issue){
    if (issue == NULL){
        issue = "No args found.";
    }
    char* repeat(char item, int count){
        char* buff = malloc(count + 1);
        if (!buff){
            printf("Failed to allocate memory to repeat text.\n");
            return NULL;
        }
        for (int index = 0; index < count; index++){
            buff[index] = item;
        }
        buff[count] = '\0';
        return buff;
    }
    char* rep = repeat('=', strlen(issue));
    if (!rep){
        return;
    }
    printf("=====%s=====\n", rep);
    printf("==== %s ====\n", issue);
    printf("=====%s=====\n", rep);
    free(rep);
    printf("\n");
    printf("cleanai --new\n");
    printf("             --config path/to/config.json\n");
    printf("                                         --train\n");
    printf("                                                   [--pretrain]\n");
    printf("                                         --pretrain\n");
    printf("                                                   [--train]\n");
    printf("        --load path/to/model.zip\n");
    printf("                                [--config path/to/config.json]\n");
    printf("                                                              [--train]\n");
    printf("                                                                          [--pretrain]\n");
    printf("                                                              [--pretrain]\n");
    printf("                                                                          [--train]\n");
    printf("        --init-config path/to/new/config.json\n");
    printf("\n");
    printf("Note: Arguments between square brackets ([...]) are optional.\n");
}

bool file_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Failed to open file at path \"%s\".\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("Failed to allocate memory to read file at path \"%s\".\n", path);
        fclose(f);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, f);
    fclose(f);

    if (read != size) {
        printf("Failed to read file at path \"%s\".\n", path);
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0'; // null-terminate for safety
    return buffer;
}

bool file_write(char* path, char* content) {
    FILE* file = fopen(path, "w");
    if (!file) {
        return false;
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    
    return written == len;
}

#ifdef _WIN32
    HANDLE timer_mutex = NULL;
    void init_timer_mutex() {
        timer_mutex = CreateMutexA(NULL, FALSE, NULL);
    }
    void lock_timer() { WaitForSingleObject(timer_mutex, INFINITE); }
    void unlock_timer() { ReleaseMutex(timer_mutex); }
#else
    pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
    void init_timer_mutex() {}
    void lock_timer() { pthread_mutex_lock(&timer_mutex); }
    void unlock_timer() { pthread_mutex_unlock(&timer_mutex); }
#endif
int main(int argc, char** argv){
    init_timer_mutex();

    int* ids = malloc(sizeof(int)); //initial dummy slot

    if (!ids){
        printf("Failed memory allocation for ids, ids will not be tracked therefore ids may not be strictly unique.\n");
    }

    int ids_len = 0;

    srand(time(NULL));

    int genid(){
        int id;
        while (true){
            id = rand() % 100000;

            if (!ids){
                return id;
            }

            if (ids_len == 100000){
                printf("All possible ids combinations exhaused, this id will be a duplicate of another id.\n");
                return id;
            }

            bool found = false;
            for (int index = 0; index < ids_len; index++){
                if (ids[index] == id){
                    found = true;
                    break;
                }
            }

            if (!found){
                break;
            }
        }
        //grw
        int* tmp_ids = realloc(ids, (ids_len + 1) * sizeof(int));
        if (!tmp_ids){
            printf("Failed memory allocation to grow ids list.\n");
            return id; //better to return something than nothing
        }
        ids = tmp_ids;

        ids[ids_len] = id;
        ids_len++;
        return id;
    }

    int freeId(int id){
        int indexOfId = -1;
        for (int index = 0; index < ids_len; index++){
            if (id == ids[index]){
                indexOfId = index;
                break;
            }
        }
        if (indexOfId == -1){
            return -1; //id not found
        }

        //shrnk
        int* n_ids = malloc((ids_len - 1) * sizeof(int));
        if (!n_ids){
            printf("Failed to allocate memory to shrink ids list.\n");
            return -1;
        }

        int offset = 0;
        for (int index = 0; index < ids_len; index++){
            if (index == indexOfId){
                offset += 1;
                continue;
            }
            n_ids[index - offset] = ids[index];
        }

        free(ids);
        ids = n_ids;
        ids_len--;
        return 0;
    }

    long long** timers = malloc(sizeof(long long*)); //initial dummy slot
    
    if (!timers){
        printf("Failed memory allocation for timers, timers will not be available.\n");
    }

    int timers_len = 0; //0 elements

    long long timer(){
        lock_timer();
        
        if (!timers){
            unlock_timer();
            return -1;
        }

        long long curr_time = time_ms();
        long long id = (long long)(genid());
        
        long long** tmp_timers = realloc(timers, (timers_len + 1) * sizeof(long long*));
        if (!tmp_timers){
            printf("Failed memory allocation to grow timers list.\n");
            unlock_timer();
            return -1;
        }
        timers = tmp_timers;
        timers[timers_len] = malloc(2 * sizeof(long long));
        if (!timers[timers_len]){
            printf("Failed memory allocation to grow timers list.\n");
            unlock_timer();
            return -1;
        }
        timers[timers_len][0] = id;
        timers[timers_len][1] = curr_time;
        timers_len++;
        
        unlock_timer();
        return id;
    }

    long long timer_end(long long timer_id){
        lock_timer();
        
        int indexOfTimer = -1;
        long long timer_time;
        for (int index = 0; index < timers_len; index++){
            if (timers[index][0] == timer_id){
                indexOfTimer = index;
                timer_time = time_ms() - timers[index][1];
                break;
            }
        }
        if (indexOfTimer == -1){
            unlock_timer();
            return -1;
        }

        long long** new_timers = malloc((timers_len - 1) * sizeof(long long*));
        if (!new_timers){
            printf("Failed memory allocation to shrink timers list.\n");
            unlock_timer();
            return -1;
        }

        int offset = 0;
        for (int index = 0; index < timers_len; index++){
            if (index == indexOfTimer){
                offset++;
                continue;
            }
            new_timers[index - offset] = malloc(2 * sizeof(long long));
            if (!new_timers[index - offset]){
                printf("Failed memory allocation to shrink timers list.\n");
                for (int subindex = 0; subindex < timers_len - index - offset; subindex++){
                    free(new_timers[subindex]);
                }
                free(new_timers);
                unlock_timer();
                return -1;
            }
            new_timers[index - offset][0] = timers[index][0];
            new_timers[index - offset][1] = timers[index][1];
        }

        for (int index = 0; index < timers_len; index++){
            free(timers[index]);
        }
        free(timers);
        timers = new_timers;
        timers_len--;
        
        freeId((int)(timer_id));
        
        unlock_timer();
        return timer_time;
    }

    //store filename separately
    char* self_filename = malloc(strlen(argv[0]) + 1);
    if (!self_filename){
        printf("Failed to allocate memory to parse args.\n");
        return 1;
    }
    strcpy(self_filename, argv[0]);
    //remove file name from argv
    char** nargv = malloc((argc + 1) * sizeof(char*));
    if (!nargv){
        printf("Failed to allocate memory to parse args.\n");
        return 1;
    }
    nargv[argc] = NULL;
    for (int index = 1; index < argc; index++){
        nargv[index - 1] = malloc(strlen(argv[index]) + 1);
        if (!nargv[index - 1]){
            printf("Failed to allocate memory to parse args.\n");
            return 1; //os will auto reclaim memory
        }
        strcpy(nargv[index - 1], argv[index]);
    }
    argv = nargv;
    argc--;

    //parse arguments
    if (argc == 1){
        char* arg = argv[0];
        if (strcmp(arg, "--init-config") == 0){
            help("You need to specify a path to create the new config file with --init-config.");
            return 1;
        }
    }
    if (argc == 2){
        char* arg_a = argv[0];
        char* arg_b = argv[1];
        if (strcmp(arg_a, "--init-config") == 0){
            printf("Arguments parsed successfully :)\n");
            
        init_config_file_exists_check:
            if (file_exists(arg_b)){
                while (true){
                    char prompt_str[512];
                    sprintf(prompt_str, "File '%s' already exists, do you want to allow the program to overwrite it? (y/n) ", arg_b);
                    char* overwrite_prompt = input(prompt_str);
                    if (!overwrite_prompt){
                        printf("Failed to read user input.\n");
                        return 1;
                    }

                    if (strcmp(overwrite_prompt, "y") == 0){
                        break;
                    }
                    else{
                        if (strcmp(overwrite_prompt, "n") == 0){
                            char* new_arg_b = input("Please specify a new file to save as: ");
                            if (!new_arg_b){
                                printf("Failed to read user input.\n");
                                return 1;
                            }
                            arg_b = new_arg_b;
                            free(overwrite_prompt);
                            goto init_config_file_exists_check;
                        }
                        else{
                            printf("Invalid input.\n");
                            free(overwrite_prompt);
                            continue;
                        }
                    }
                }
            }

            cJSON* new_config = cJSON_CreateObject();
            printf("--- Model architecture parameters (required) ---\n");
            
            int heads_n = 0;
            printf("Heads is a paremeter that if higher, while costing more compute, allows your model to focus on more different parts of inputs at the same time. Common values range from 2 to 4 for small models, 8 to 16 for medium models and 16-64+ for large models.\n");
            while (true){
                char* head_prompt = input("Choose heads (integer, >=1)? ");
                if (!head_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                heads_n = atoi(head_prompt);
                free(head_prompt);
                if (heads_n < 1){
                    printf("Invalid input, must be an integer >= 1.\n");
                    continue;
                }

                cJSON_AddNumberToObject(new_config, "heads", heads_n);
                break;
            }

            printf("\n");

            printf("EmbeddingSize is a parameter that if higher, while costing more compute, allows your model to memorize more information. Common values range from 64 to 128 for small models, 256 to 512 for medium models and 1024 to 12768+ for large models.\n");
            printf("IMPORTANT: embeddingSize must be divisible by heads.\n");
            int embeddingSize_n = 0;
            while (true){
                char prompt_str[256];
                sprintf(prompt_str, "Choose embeddingSize (integer, >=1, divisible by %d)? ", heads_n);
                char* embeddingSize_prompt = input(prompt_str);
                if (!embeddingSize_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                embeddingSize_n = atoi(embeddingSize_prompt);
                free(embeddingSize_prompt);
                if ((embeddingSize_n < 1) || ((int)(embeddingSize_n / heads_n) != ((float)(embeddingSize_n) / (float)(heads_n)))){
                    printf("Invalid input, must be an integer >= 1 and divisible by %d.\n", heads_n);
                    continue;
                }

                cJSON_AddNumberToObject(new_config, "embeddingSize", embeddingSize_n);
                break;
            }

            printf("\n");

            int layers_n = 0;
            printf("layersAmount is a parameter that if higher, while costing more compute, allows your model to think through problems in more steps. Common values range from 2 to 4 for small models, 4 to 8 for medium models and 8-32+ for large models.\n");
            while (true){
                char *layer_prompt = input("Choose layersAmount (integer, >=1)? ");
                if (!layer_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                layers_n = atoi(layer_prompt);
                free(layer_prompt);
                if (layers_n < 1){
                    printf("Invalid input, must be an integer >= 1.\n");
                    continue;
                }
                cJSON_AddNumberToObject(new_config, "layersAmount", layers_n);
                break;
            }

            printf("\n");

            int ffn_grow_size = 0;
            printf("ffnGrowSize is a parameter that if higher, while costing more compute, gives your model better implicit reasoning. Common values range from 4 to 8 for small models, 8 to 12 for medium models and 16-64+ for large models.\n");
            while (true){
                char *ffn_prompt = input("Choose ffnGrowSize (integer, >=1)? ");
                if (!ffn_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                ffn_grow_size = atoi(ffn_prompt);
                free(ffn_prompt);
                if (ffn_grow_size < 1){
                    printf("Invalid input, must be an integer >= 1.\n");
                    continue;
                }
                cJSON_AddNumberToObject(new_config, "ffnGrowSize", ffn_grow_size);
                break;
            }
            
            printf("\n");

            int context_size = 0;
            printf("contextSize is the maximum amount of tokens the model can process at once. Common values range from 128 to 512 for small models, 2048 to 8192 for medium models and 32768-1000000+ for large models.\n");
            while (true){
                char *context_prompt = input("Choose contextSize (integer, >=1)? ");
                if (!context_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                context_size = atoi(context_prompt);
                free(context_prompt);
                if (context_size < 1){
                    printf("Invalid input, must be an integer >= 1.\n");
                    continue;
                }
                cJSON_AddNumberToObject(new_config, "contextSize", context_size);
                break;
            }

            printf("\n");

            int max_output_size = 0;
            printf("maxOutputSize is the maximum amount of tokens the model can output in one response. Common values range from 16 to 128 for small models, 512 to 4096 for medium models and 16384-65536+ for large models.\n");
            while (true){
                char *output_prompt = input("Choose maxOutputSize (integer, >=1)? ");
                if (!output_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                max_output_size = atoi(output_prompt);
                free(output_prompt);
                if (max_output_size < 1){
                    printf("Invalid input, must be an integer >= 1.\n");
                    continue;
                }
                cJSON_AddNumberToObject(new_config, "maxOutputSize", max_output_size);
                break;
            }

            printf("\n");

            void cJSON_AddNumberToArray(cJSON* arr, double n){
                cJSON_AddItemToArray(arr, cJSON_CreateNumber(n));
                return;
            }

            double embedding_start = 0;
            double embedding_end = 0;
            printf("embeddinginitrange is the range of random values to initialize embeddings within. Smaller ranges like [-0.01, 0.01] make training more stable but slower, larger ranges like [-0.1, 0.1] make it faster but less stable. Enter start and end values (floats).\n");
            while (true){
                char *embedding_start_prompt = input("Choose embeddinginitrange start (float)? ");
                if (!embedding_start_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                embedding_start = atof(embedding_start_prompt);
                free(embedding_start_prompt);
                
                char *embedding_end_prompt = input("Choose embeddinginitrange end (float)? ");
                if (!embedding_end_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                embedding_end = atof(embedding_end_prompt);
                free(embedding_end_prompt);
                
                if (embedding_start >= embedding_end){
                    printf("Invalid input, start must be less than end.\n");
                    continue;
                }
                
                cJSON* embedding_array = cJSON_CreateArray();
                cJSON_AddNumberToArray(embedding_array, embedding_start);
                cJSON_AddNumberToArray(embedding_array, embedding_end);
                cJSON_AddItemToObject(new_config, "embeddinginitrange", embedding_array);
                break;
            }

            printf("\n");

            double bias_start = 0;
            double bias_end = 0;
            printf("biasesinitrange is the range of random values to initialize biases within. Smaller ranges like [-0.01, 0.01] make training more stable but slower, larger ranges like [-0.1, 0.1] make it faster but less stable. Enter start and end values (floats).\n");
            while (true){
                char *bias_start_prompt = input("Choose biasesinitrange start (float)? ");
                if (!bias_start_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                bias_start = atof(bias_start_prompt);
                free(bias_start_prompt);
                
                char *bias_end_prompt = input("Choose biasesinitrange end (float)? ");
                if (!bias_end_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                bias_end = atof(bias_end_prompt);
                free(bias_end_prompt);
                
                if (bias_start >= bias_end){
                    printf("Invalid input, start must be less than end.\n");
                    continue;
                }
                
                cJSON* bias_array = cJSON_CreateArray();
                cJSON_AddNumberToArray(bias_array, bias_start);
                cJSON_AddNumberToArray(bias_array, bias_end);
                cJSON_AddItemToObject(new_config, "biasesinitrange", bias_array);
                break;
            }

            printf("\n\n");

            bool any_train_param_config_setup = false;
            printf("Pretraining is training the model on unstructured language like wikipedia dumps where there are no conversations, the goal is to teach the model to predict language in general.\n");
            while (true){
                char* pretrain_setup_prompt = input("Do you wish to setup the pretraining parameters of the config? (y/n) ");
                if (!pretrain_setup_prompt){
                    printf("Failed to read user input.\n");
                    return 1;
                }

                if (strcmp(pretrain_setup_prompt, "y") == 0){
                    any_train_param_config_setup = true;
                    break;
                }
                else{
                    if (strcmp(pretrain_setup_prompt, "n") == 0){
                        goto after_pretrain_config_setup;
                    }
                    else{
                        printf("Invalid input, type 'y' for yes or 'n' for no.\n");
                        continue;
                    }
                }
            }
            
            printf("--- Pretraining config parameters setup ---\n");
            cJSON* paths_array = cJSON_CreateArray();
            printf("pre-training-paths are the file paths to your pretraining data. Enter each path one at a time, press enter with no input when done.\n");
            while (true){
                char* path = input("Enter pre-training path? ");
                if (!path || strlen(path) == 0){
                    free(path);
                    break;
                }
                cJSON_AddItemToArray(paths_array, cJSON_CreateString(path));
                free(path);
            }
            cJSON_AddItemToObject(new_config, "pre-training-paths", paths_array);

            printf("\n");

            int pretrain_epochs = 0;
            printf("pre-train-epochs is the number of times the program should train on the pretraining data. Higher means more training but takes longer.\n");
            while (true){
                char* epoch_prompt = input("Choose pre-train-epochs (integer, >1)? ");
                if (!epoch_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                pretrain_epochs = atoi(epoch_prompt);
                free(epoch_prompt);
                if (pretrain_epochs < 1){
                    printf("Invalid input, must be an integer > 1.\n");
                    continue;
                }
                cJSON_AddNumberToObject(new_config, "pre-train-epochs", pretrain_epochs);
                break;
            }

            printf("\n");

            char* pretrain_optimizer = NULL;
            printf("pre-train-optimizer is the optimizer used to train the model. Adam is generally best for everything, SGD is older and more granular, sgd_momentum is between both.\n");
            while (true){
                char* opt_prompt = input("Choose pre-train-optimizer (adam, sgd, sgd_momentum)? ");
                if (!opt_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                if (strcmp(opt_prompt, "adam") == 0 || strcmp(opt_prompt, "sgd") == 0 || strcmp(opt_prompt, "sgd_momentum") == 0){
                    pretrain_optimizer = opt_prompt;
                    cJSON_AddStringToObject(new_config, "pre-train-optimizer", pretrain_optimizer);
                    break;
                } else {
                    printf("Invalid input, must be adam, sgd, or sgd_momentum.\n");
                    free(opt_prompt);
                    continue;
                }
            }

            printf("\n\n");

        after_pretrain_config_setup:
            printf("Training is training the model on structured language like conversational exanges, the goal is to teach the model to use the learned pretraining language skills to reason and comunicate. This requires structured json datasets.\n");

            while (true){
                char* train_setup_prompt = input("Do you wish to setup the training parameters of the config? (y/n) ");
                if (!train_setup_prompt){
                    printf("Failed to read user input.\n");
                    return 1;
                }

                if (strcmp(train_setup_prompt, "y") == 0){
                    any_train_param_config_setup = true;
                    break;
                }
                else{
                    if (strcmp(train_setup_prompt, "n") == 0){
                        goto after_train_config_setup;
                    }
                    else{
                        printf("Invalid input, type 'y' for yes or 'n' for no.\n");
                        continue;
                    }
                }
            }

            printf("--- Training config parameters setup ---\n");

            cJSON* train_paths_array = cJSON_CreateArray();
            printf("training-dataset-paths are the file paths to your training data. Enter each path one at a time, press enter with no input when done.\n");
            while (true){
                char* train_path = input("Enter training-dataset-path? ");
                if (!train_path || strlen(train_path) == 0){
                    free(train_path);
                    break;
                }
                cJSON_AddItemToArray(train_paths_array, cJSON_CreateString(train_path));
                free(train_path);
            }
            cJSON_AddItemToObject(new_config, "training-dataset-paths", train_paths_array);

            printf("\n");

            int train_epochs = 0;
            printf("train-epochs is the number of times the program should train on the training data. Higher means more training but takes longer.\n");
            while (true){
                char* train_epoch_prompt = input("Choose train-epochs (integer, >1)? ");
                if (!train_epoch_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                train_epochs = atoi(train_epoch_prompt);
                free(train_epoch_prompt);
                if (train_epochs < 1){
                    printf("Invalid input, must be an integer > 1.\n");
                    continue;
                }
                cJSON_AddNumberToObject(new_config, "train-epochs", train_epochs);
                break;
            }

            printf("\n");

            char* train_optimizer = NULL;
            printf("train-optimizer is the optimizer used to train the model. Adam is generally best for everything, SGD is older and more granular, sgd_momentum is between both.\n");
            while (true){
                char* train_opt_prompt = input("Choose train-optimizer (adam, sgd, sgd_momentum)? ");
                if (!train_opt_prompt){
                    printf("Failed to read user input\n");
                    return 1;
                }
                if (strcmp(train_opt_prompt, "adam") == 0 || strcmp(train_opt_prompt, "sgd") == 0 || strcmp(train_opt_prompt, "sgd_momentum") == 0){
                    train_optimizer = train_opt_prompt;
                    cJSON_AddStringToObject(new_config, "train-optimizer", train_optimizer);
                    break;
                } else {
                    printf("Invalid input, must be adam, sgd, or sgd_momentum.\n");
                    free(train_opt_prompt);
                    continue;
                }
            }

        after_train_config_setup:
            if (any_train_param_config_setup){
                printf("--- Training parameters applied to both pretraining and training ---\n");
                double learning_rate = 0;
                printf("learningRate controls how much the model adjusts its weights each training step. High learning rates learn faster but can skip over important details and make the model regress, while too low can get stuck in mediocrity loops and be slow. Common values range from 0.0001 to 0.01.\n");
                while (true){
                    char* lr_prompt = input("Choose learningRate (float)? ");
                    if (!lr_prompt){
                        printf("Failed to read user input\n");
                        return 1;
                    }
                    learning_rate = atof(lr_prompt);
                    free(lr_prompt);
                    cJSON_AddNumberToObject(new_config, "learningRate", learning_rate);
                    break;
                }

                printf("\n");

                double learning_rate_decay = 0;
                printf("learningRateDecay is a factor that the learning rate is multiplied by when loss has not improved for learningRateDecayPatience epochs. This helps prevent getting stuck and fine-tunes training. Common values range from 0.5 to 0.99.\n");
                while (true){
                    char* decay_prompt = input("Choose learningRateDecay (float, 0-1)? ");
                    if (!decay_prompt){
                        printf("Failed to read user input\n");
                        return 1;
                    }
                    learning_rate_decay = atof(decay_prompt);
                    free(decay_prompt);
                    if (learning_rate_decay <= 0 || learning_rate_decay >= 1){
                        printf("Invalid input, must be a float between 0 and 1.\n");
                        continue;
                    }
                    cJSON_AddNumberToObject(new_config, "learningRateDecay", learning_rate_decay);
                    break;
                }

                printf("\n");

                int learning_rate_decay_patience = 0;
                printf("learningRateDecayPatience is the number of epochs the loss must not improve before the learning rate is decayed.\n");
                while (true){
                    char* patience_prompt = input("Choose learningRateDecayPatience (integer, >=1)? ");
                    if (!patience_prompt){
                        printf("Failed to read user input\n");
                        return 1;
                    }
                    learning_rate_decay_patience = atoi(patience_prompt);
                    free(patience_prompt);
                    if (learning_rate_decay_patience < 1){
                        printf("Invalid input, must be an integer >= 1.\n");
                        continue;
                    }
                    cJSON_AddNumberToObject(new_config, "learningRateDecayPatience", learning_rate_decay_patience);
                    break;
                }

                printf("\n");

                int batch_size = 0;
                printf("batchSize is the number of training tasks spawned per batch, with each task running as a thread. Be careful as larger batch sizes mean more parallelism but also more memory usage. Larger batch sizes also mean more averaged and stable training. Common values range from 4 to 64.\n");
                while (true){
                    char* batch_prompt = input("Choose batchSize (integer, >=1)? ");
                    if (!batch_prompt){
                        printf("Failed to read user input\n");
                        return 1;
                    }
                    batch_size = atoi(batch_prompt);
                    free(batch_prompt);
                    if (batch_size < 1){
                        printf("Invalid input, must be an integer >= 1.\n");
                        continue;
                    }
                    cJSON_AddNumberToObject(new_config, "batchSize", batch_size);
                    break;
                }

                printf("\n");

                char* anti_overfitting = NULL;
                printf("antiOverfittingOptimisations allows the model to have better generalization and avoid learning the data perfectly. Instead it makes the model generalize and use the data properly. Enter true or false.\n");
                while (true){
                    char* anti_prompt = input("Choose antiOverfittingOptimisations (true/false)? ");
                    if (!anti_prompt){
                        printf("Failed to read user input\n");
                        return 1;
                    }
                    if (strcmp(anti_prompt, "true") == 0 || strcmp(anti_prompt, "false") == 0){
                        cJSON_AddBoolToObject(new_config, "antiOverfittingOptimisations", strcmp(anti_prompt, "true") == 0);
                        free(anti_prompt);
                        break;
                    } else {
                        printf("Invalid input, must be true or false.\n");
                        free(anti_prompt);
                        continue;
                    }
                }

                printf("\n");

                char* autosave = NULL;
                printf("autosave makes the program save the model each epoch automatically, so you don't lose progress if training is interrupted. Enter true or false.\n");
                while (true){
                    char* autosave_prompt = input("Choose autosave (true/false)? ");
                    if (!autosave_prompt){
                        printf("Failed to read user input\n");
                        return 1;
                    }
                    if (strcmp(autosave_prompt, "true") == 0 || strcmp(autosave_prompt, "false") == 0){
                        cJSON_AddBoolToObject(new_config, "autosave", strcmp(autosave_prompt, "true") == 0);
                        free(autosave_prompt);
                        break;
                    } else {
                        printf("Invalid input, must be true or false.\n");
                        free(autosave_prompt);
                        continue;
                    }
                }
            }
            printf("\n");

            char* new_config_text = cJSON_Print(new_config);
            if (!new_config_text){
                printf("Failed to make the new config into text.\n");
                return 1;
            }
            cJSON_Delete(new_config);

            long long newconf_write_timer = timer();
            printf("Writing new config to '%s'...\n", arg_b);

            bool success = file_write(arg_b, new_config_text);
            if (!success){
                printf("Failed to write new config to '%s' in %lldms.\n", arg_b, timer_end(newconf_write_timer));
                return 1;
            }
            printf("Wrote new config to '%s' in %lldms.\n", arg_b, timer_end(newconf_write_timer));
            return 0;
        }
    }

    bool do_pretrain = false;
    bool do_train = false;
    bool new = false;
    bool load = false;
    bool debug = false;
    int head_dim = -1;
    int embeddingSize = -1;
    int heads = -1;

    char* valid_flags[] = {"--new", "--load", "--config", "--train", "--pretrain", "--debug", NULL};
    int valid_flags_len = 0;
    while (true){
        if (!(valid_flags[valid_flags_len] == NULL)){
            valid_flags_len++;
        }
        else{
            break;
        }
    }
    
    char* config_location = malloc(1); //dummy init
    bool config_init = false;
    char* model_location = malloc(1); //dummy init
    if (!config_location){
        printf("Failed to allocate memory to parse args.\n");
        return -1;
    }
    if (!model_location){
        printf("Failed to allocate memory to parse args.\n");
        return -1;
    }

    config_location[0] = '\0'; //if printed safe anyways :)
    model_location[0] = '\0';

    if (argc == 0){
        help(NULL);
        return 0;
    }
    
    bool nextIsVal = false;
    for (int index = 0; index < argc; index++){
        char* arg = argv[index];
        if (nextIsVal){
            nextIsVal = false;
            continue;
        }
        if (strcmp(arg, "--new") == 0){
            if (new){
                help("You can't specify --new multiple times.");
                return 0;
            }
            if (load){
                help("You can't specify --new and --load at the same time.");
                return 0;
            }
            new = true;
        }
        else{
            if (strcmp(arg, "--load") == 0){
                if (load){
                    help("You can't specify --load multiple times.");
                    return 0;
                }
                if (new){
                    help("You can't specify --load and --new at the same time.");
                    return 0;
                }
                load = true;
                if (argc - index - 1 == 0){
                    help("You need to specify a model file path after --load.");
                    return 0;
                }
                nextIsVal = true;
                char* nextArg = argv[index + 1];
                for (int subindex = 0; subindex < valid_flags_len; subindex++){
                    if (strcmp(nextArg, valid_flags[subindex]) == 0){
                        nextIsVal = false;
                        break;
                    }
                }
                if (!nextIsVal){
                    help("You need to specify a model file path after --load.");
                    return 0;
                }
                model_location = realloc(model_location, strlen(nextArg) + 1);
                if (!model_location){ //no need to use tmp we exit if fail anyways
                    printf("Failed to allocate memory to parse args.\n");
                    return 1;
                }
                strcpy(model_location, nextArg);
            }
            else{
                if (strcmp(arg, "--train") == 0){
                    if (do_train){
                        help("You can't specify --train multiple times.");
                        return 0;
                    }
                    do_train = true;
                }
                else{
                    if (strcmp(arg, "--pretrain") == 0){
                        if (do_pretrain){
                            help("You can't specify --pretrain multiple times.");
                            return 0;
                        }
                        do_pretrain = true;
                    }
                    else{
                        if (strcmp(arg, "--config") == 0){
                            if (config_init){
                                help("You can't specify --config multiple times.");
                                return 0;
                            }
                            config_init = true;
                            if (argc - index - 1 == 0){
                                help("You need to specify a config file path after --config.");
                                return 0;
                            }
                            nextIsVal = true;
                            char* nextArg = argv[index + 1];
                            for (int subindex = 0; subindex < valid_flags_len; subindex++){
                                if (strcmp(nextArg, valid_flags[subindex]) == 0){
                                    nextIsVal = false;
                                    break;
                                }
                            }
                            if (!nextIsVal){
                                help("You need to specify a config file path after --config.");
                                return 0;
                            }
                            config_location = realloc(config_location, strlen(nextArg) + 1);
                            if (!config_location){
                                printf("Failed to allocate memory to parse args.\n");
                                return 1;
                            }
                            strcpy(config_location, nextArg);
                        }
                        else{
                            if (strcmp(arg, "--debug") == 0){
                                if (debug){
                                    help("You can't specify --debug multiple times.");
                                    return 0;
                                }
                                else{
                                    debug = true;
                                }
                            }
                            else{
                                int help_message_len = strlen("Arg \"") + strlen(arg) + strlen("\" is invalid.") + 1;
                                char* help_message = malloc(help_message_len);
                                if (!help_message){
                                    printf("Failed to allocate memory to parse args.\n");
                                    return 1;
                                }
                                sprintf(help_message, "Arg \"%s\" is invalid.", arg);
                                help(help_message);
                                return 0;
                            }
                        }
                    }
                }
            }
        }
    }

    if (new){
        if ((!do_pretrain) && (!do_train)){
            help("You need to specify either or both --pretrain and --train with --new.");
            return 0;
        }
    }

    if (new){
        if (!config_init){
            help("You need to specify a config file path with --config.");
            return 0;
        }
    }

    if (load){
        if (do_train || do_pretrain){
            if (!config_init){
                help("You need to specify a config file path with --config.");
                return 0;
            }
        }
    }

    if (config_location){
        if (strlen(config_location) > 0){
            if (!file_exists(config_location)){
                printf("Failed to open config file. Most likely causes are that it doesn't exist or that you don't have the required permissions to read it.\n");
                return 1;
            }
        }
    }

    if (load){
        if (!file_exists(model_location)){
            printf("Failed to open model file. Most likely causes are that it doesn't exist or that you don't have the required permissions to read it.\n");
            return -1;
        }
    }

    printf("Arguments parsed successfully :)\n");

    int contextSize = -1;
    int maxOutputSize = -1;
    if (load && (!do_pretrain) && (!do_train)){
        if (config_location){
            if (strlen(config_location) > 0){
                goto parse_config;
            }
        }
        printf("Since you are loading a model and not (pre-)training it, a config is optional and you did not provide one, a context size and max output size are required to use the model.\n");
        while (contextSize < 1){
            char* contextSizeInput = input("Please enter context size for the model (>1): ");
            if (!contextSizeInput){
                printf("Failed to read user input.\n");
                return 1;
            }
            contextSize = atoi(contextSizeInput);
            free(contextSizeInput);

            if (contextSize < 1){
                printf("Invalid input, should be integer greater than 1.\n");
                continue;
            }
        }
        while (maxOutputSize < 1){
            char* maxOutputSizeInput = input("Please enter max output size for the model (>1): ");
            if (!maxOutputSizeInput){
                printf("Failed to read user input.\n");
                return 1;
            }
            maxOutputSize = atoi(maxOutputSizeInput);
            free(maxOutputSizeInput);

            if (maxOutputSize < 1){
                printf("Invalid input, should be integer greater than 1.\n");
                continue;
            }
        }
        goto after_config_parse;
    }

parse_config:
    printf("Reading config file...\n");
    char* config_file = read_file(config_location);
    if (!config_file){
        printf("Failed to read config file.\n");
        return 1;
    }
    //try to parse json
    cJSON* config = cJSON_Parse(config_file);
    if (!config){
        printf("Failed to parse config. Common cause: corrupted json.\n");
        return 1;
    }
    free(config_file);

    bool isInt(double n){
        if ((int)(n) == n){
            return true;
        }
        else{
            return false;
        }
    }

    bool isFloat(double n){
        return !isInt(n);
    }

    cJSON* pre_training_paths_raw = cJSON_GetObjectItem(config, "pre-training-paths");
    char** pre_training_paths = NULL;
    int pre_training_paths_len = -1;
    if (!cJSON_IsArray(pre_training_paths_raw)){
        if (!do_pretrain){
            printf("[Config] [Warning] pre-training-paths is missing/corrupted but --pretrain is not specified therefore this can be ignored.\n");
        }
        else{
            printf("[Config] [Fatal] pre-training-paths is missing/corrupted.\n");
            return 1;
        }
    }
    else{
        if (!do_pretrain){
            printf("[Config] [Info] Ignoring pre-training-paths as you did not specify --pretrain.\n");
        }
        else{
            pre_training_paths_len = cJSON_GetArraySize(pre_training_paths_raw);
            if (pre_training_paths_len == 0){
                printf("[Config] [Fatal] pre-training-paths is empty.\n");
                return 1;
            }
            pre_training_paths = malloc(pre_training_paths_len * sizeof(char*));
            if (!pre_training_paths){
                printf("Failed to allocate memory to parse config.\n");
                return 1;
            }
            for (int index = 0; index < pre_training_paths_len; index++){
                cJSON* item = cJSON_GetArrayItem(pre_training_paths_raw, index);
                if (!cJSON_IsString(item)){
                    printf("[Config] [Fatal] Item %d/%d of pre-training-paths is not a string.\n", index + 1, pre_training_paths_len);
                    return 1;
                }
                pre_training_paths[index] = malloc(strlen(item->valuestring) + 1);
                if (!pre_training_paths[index]){
                    printf("Failed to allocate memory to parse config.\n");
                    return 1;
                }
                strcpy(pre_training_paths[index], item->valuestring);
                if (strlen(pre_training_paths[index]) == 0){
                    printf("[Config] [Fatal] Item %d/%d of pre-training-paths isn't a valid file path.\n", index + 1, pre_training_paths_len);
                    return 1;
                }
                if (!file_exists(pre_training_paths[index])){
                    printf("[Config] [Fatal] Item %d/%d of pre-training-paths isn't a valid file path.\n", index + 1, pre_training_paths_len);
                    return 1;
                }
            }
        }
    }

    cJSON* training_dataset_paths_raw = cJSON_GetObjectItem(config, "training-dataset-paths");
    char** training_dataset_paths = NULL;
    int training_dataset_paths_len = -1;
    if (!cJSON_IsArray(training_dataset_paths_raw)){
        if (!do_train){
            printf("[Config] [Warning] training-dataset-paths is missing/corrupted but --train is not specified therefore this can be ignored.\n");
        }
        else{
            printf("[Config] [Fatal] training-dataset-paths is missing/corrupted.\n");
            return 1;
        }
    }
    else{
        if (!do_train){
            printf("[Config] [Info] Ignoring training-dataset-paths as you did not specify --train.\n");
        }
        else{
            training_dataset_paths_len = cJSON_GetArraySize(training_dataset_paths_raw);
            if (training_dataset_paths_len == 0){
                printf("[Config] [Fatal] training-dataset-paths is empty.\n");
                return 1;
            }

            training_dataset_paths = malloc(training_dataset_paths_len * sizeof(char*));
            if (!training_dataset_paths){
                printf("Failed to allocate memory to parse config.\n");
                return 1;
            }

            for (int index = 0; index < training_dataset_paths_len; index++){
                cJSON* item = cJSON_GetArrayItem(training_dataset_paths_raw, index);
                if (!cJSON_IsString(item)){
                    printf("[Config] [Fatal] Item %d/%d of training-dataset-paths is not a string.\n", index + 1, training_dataset_paths_len);
                    return 1;
                }

                training_dataset_paths[index] = malloc(strlen(item->valuestring) + 1);
                if (!training_dataset_paths[index]){
                    printf("Failed to allocate memory to parse config.\n");
                    return 1;
                }

                strcpy(training_dataset_paths[index], item->valuestring);

                if (strlen(training_dataset_paths[index]) == 0){
                    printf("[Config] [Fatal] Item %d/%d of training-dataset-paths isn't a valid file path.\n", index + 1, training_dataset_paths_len);
                    return 1;
                }

                if (!file_exists(training_dataset_paths[index])){
                    printf("[Config] [Fatal] Item %d/%d of training-dataset-paths isn't a valid file path.\n", index + 1, training_dataset_paths_len);
                    return 1;
                }
            }
        }
    }

    cJSON* pre_train_epochs_raw = cJSON_GetObjectItem(config, "pre-train-epochs");
    int pre_train_epochs = -1;
    if (!cJSON_IsNumber(pre_train_epochs_raw)){
        if (do_pretrain){
            printf("[Config] [Fatal] pre-train-epochs is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] pre-train-epochs is missing/corrupted but you did not specify --pretrain therefore this can be ignored.\n");
        }
    }
    else{
        if (!(isInt(pre_train_epochs_raw->valuedouble))){
            if (do_pretrain){
                printf("[Config] [Fatal] pre-train-epochs is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] pre-train-epochs is supposed to be an int but it is a float, as you did not specify --pretrain this can be ignored.\n");
            }
        }
        else{
            if ((int)(pre_train_epochs_raw->valuedouble) < 1){
                if (do_pretrain){
                    printf("[Config] [Fatal] pre-train-epochs is supposed to be >= 1, but it is set to %d.\n", (int)(pre_train_epochs_raw->valuedouble));
                    return -1;
                }
                else{
                    printf("[Config] [Warning] pre-train-epochs is supposed to be >= 1, but it is set to %d. However as you did not specify --pretrain this can be ignored.\n", (int)(pre_train_epochs_raw->valuedouble));
                }
            }
            else{
                if (do_pretrain){
                    pre_train_epochs = (int)(pre_train_epochs_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring pre-train-epochs as you did not specify --pretrain.\n");
                }
            }
        }
    }

    cJSON* train_epochs_raw = cJSON_GetObjectItem(config, "train-epochs");
    int train_epochs = -1;
    if (!cJSON_IsNumber(train_epochs_raw)){
        if (do_train){
            printf("[Config] [Fatal] train-epochs is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] train-epochs is missing/corrupted but you did not specify --train therefore this can be ignored.\n");
        }
    }
    else{
        if (!(isInt(train_epochs_raw->valuedouble))){
            if (do_train){
                printf("[Config] [Fatal] train-epochs is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] train-epochs is supposed to be an int but it is a float, as you did not specify --train this can be ignored.\n");
            }
        }
        else{
            if ((int)(train_epochs_raw->valuedouble) < 1){
                if (do_train){
                    printf("[Config] [Fatal] train-epochs is supposed to be >= 1, but it is set to %d.\n", (int)(train_epochs_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] train-epochs is supposed to be >= 1, but it is set to %d. However as you did not specify --train this can be ignored.\n", (int)(train_epochs_raw->valuedouble));
                }
            }
            else{
                if (do_train){
                    train_epochs = (int)(train_epochs_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring train-epochs as you did not specify --train.\n");
                }
            }
        }
    }

    cJSON* ffn_grow_size_raw = cJSON_GetObjectItem(config, "ffnGrowSize");
    int ffn_grow_size = -1;

    if (!cJSON_IsNumber(ffn_grow_size_raw)){
        printf("[Config] [Fatal] ffnGrowSize is missing/corrupted.\n");
        return 1;
    }
    else{
        if (!(isInt(ffn_grow_size_raw->valuedouble))){
            printf("[Config] [Fatal] ffnGrowSize is supposed to be an int but it is a float.\n");
            return 1;
        }
        else{
            if ((int)(ffn_grow_size_raw->valuedouble) < 1){
                printf("[Config] [Fatal] ffnGrowSize is supposed to be >= 1, but it is set to %d.\n",
                    (int)(ffn_grow_size_raw->valuedouble));
                return 1;
            }
            else{
                ffn_grow_size = (int)(ffn_grow_size_raw->valuedouble);
            }
        }
    }

    int ffnGrowSize = ffn_grow_size;

    cJSON* pre_train_optimizer_raw = cJSON_GetObjectItem(config, "pre-train-optimizer");
    char* pre_train_optimizer = NULL;

    if (!cJSON_IsString(pre_train_optimizer_raw)){
        if (do_pretrain){
            printf("[Config] [Fatal] pre-train-optimizer is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] pre-train-optimizer is missing/corrupted but you did not specify --pretrain therefore this can be ignored.\n");
        }
    }
    else{
        if ((!(strcmp("adam", pre_train_optimizer_raw->valuestring) == 0)) && (!(strcmp("sgd_momentum", pre_train_optimizer_raw->valuestring))) && (!(strcmp("sgd", pre_train_optimizer_raw->valuestring)))){
            if (do_pretrain){
                printf("[Config] [Fatal] pre-train-optimizer is supposed to be either \"adam\", \"sgd_momentum\" or \"sgd\" but it is set to %s.\n", pre_train_optimizer_raw->valuestring);
            }
            else{
                printf("[Config] [Warning] pre-train-optimizer is supposed to be either \"adam\", \"sgd_momentum\" or \"sgd\" but it is set to %s. However as you did not specify --pretrain this can be ignored.\n", pre_train_optimizer_raw->valuestring);
            }
        }
        else{
            if (do_pretrain){
                pre_train_optimizer = malloc(strlen(pre_train_optimizer_raw->valuestring) + 1);
                if (!pre_train_optimizer){
                    printf("Failed to allocate memory to parse config.\n");
                    return 1;
                }
                else{
                    strcpy(pre_train_optimizer, pre_train_optimizer_raw->valuestring);
                }
            }
            else{
                printf("[Config] [Info] Ignoring pre-train-optimizer as you did not specify --pretrain.\n");
            }
        }
    }
    
    cJSON* train_optimizer_raw = cJSON_GetObjectItem(config, "train-optimizer");
    char* train_optimizer = NULL;

    if (!cJSON_IsString(train_optimizer_raw)){
        if (do_train){
            printf("[Config] [Fatal] train-optimizer is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] train-optimizer is missing/corrupted but you did not specify --train therefore this can be ignored.\n");
        }
    }
    else{
        if ((!(strcmp("adam", train_optimizer_raw->valuestring) == 0)) && (!(strcmp("sgd_momentum", train_optimizer_raw->valuestring))) && (!(strcmp("sgd", train_optimizer_raw->valuestring)))){
            if (do_train){
                printf("[Config] [Fatal] train-optimizer is supposed to be either \"adam\", \"sgd_momentum\" or \"sgd\" but it is set to %s.\n", train_optimizer_raw->valuestring);
            }
            else{
                printf("[Config] [Warning] train-optimizer is supposed to be either \"adam\", \"sgd_momentum\" or \"sgd\" but it is set to %s. However as you did not specify --train this can be ignored.\n", train_optimizer_raw->valuestring);
            }
        }
        else{
            if (do_train){
                train_optimizer = malloc(strlen(train_optimizer_raw->valuestring) + 1);
                if (!train_optimizer){
                    printf("Failed to allocate memory to parse config.\n");
                    return 1;
                }
                else{
                    strcpy(train_optimizer, train_optimizer_raw->valuestring);
                }
            }
            else{
                printf("[Config] [Info] Ignoring train_optimizer as you did not specify --train.\n");
            }
        }
    }

    cJSON* contextSize_raw = cJSON_GetObjectItem(config, "contextSize");

    if (!cJSON_IsNumber(contextSize_raw)){
        printf("[Config] [Fatal] contextSize is missing/corrupted.\n");
        return 1;
    }
    else{
        if (!isInt(contextSize_raw->valuedouble)){
            printf("[Config] [Fatal] contextSize is supposed to be an int but it is a float.\n");
            return 1;
        }
        else{
            if ((int)(contextSize_raw->valuedouble) < 1){
                printf("[Config] [Fatal] contextSize is supposed to be >= 1 but it is set to %d.\n", (int)(contextSize_raw->valuedouble));
                return 1;
            }
            else{
                contextSize = (int)(contextSize_raw->valuedouble);
            }
        }
    }

    cJSON* learningRate_raw = cJSON_GetObjectItem(config, "learningRate");
    double learningRate = 0xdeadbeef;
    if (!cJSON_IsNumber(learningRate_raw)){
        if (do_train || do_pretrain){
            printf("[Config] [Fatal] learningRate is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] learningRate is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (do_train || do_pretrain){
            learningRate = learningRate_raw->valuedouble;
        }
        else{
            printf("[Config] [Info] Ignoring learningRate as you did not specify either --pretrain or --train.\n");
            if (learningRate < 0){
                printf("[Console] [Info] Showing tip anyways :)\n");
            }
        }
        if (learningRate < 0){
            printf("[Config] [Tip] A learningRate < 0 will make the model unlearn the data. This is not recomended, you are on your own, good luck.\n");
        }
    }

    cJSON* maxOutputSize_raw = cJSON_GetObjectItem(config, "maxOutputSize");

    if (!cJSON_IsNumber(maxOutputSize_raw)){
        printf("[Config] [Fatal] maxOutputSize is missing/corrupted.\n");
        return 1;
    }
    else{
        if (!isInt(maxOutputSize_raw->valuedouble)){
            printf("[Config] [Fatal] maxOutputSize is supposed to be an int but it is a float.\n");
            return 1;
        }
        else{
            if ((int)(maxOutputSize_raw->valuedouble) < 1){
                printf("[Config] [Fatal] maxOutputSize is supposed to be >= 1 but it is set to %d.\n", (int)(maxOutputSize_raw->valuedouble));
                return 1;
            }
            else{
                maxOutputSize = (int)(maxOutputSize_raw->valuedouble);
            }
        }
    }

    cJSON* batchSize_raw = cJSON_GetObjectItem(config, "batchSize");
    int batchSize = -1;

    if (!cJSON_IsNumber(batchSize_raw)){
        if (do_pretrain || do_train){
            printf("[Config] [Fatal] batchSize is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] batchSize is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(batchSize_raw->valuedouble)){
            if (do_pretrain || do_train){
                printf("[Config] [Fatal] batchSize is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] batchSize is supposed to be an int but it is a float but you did not specify either --pretrain or --train therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(batchSize_raw->valuedouble) < 1){
                if (do_pretrain || do_train){
                    printf("[Config] [Fatal] batchSize is supposed to be >= 1 but it is set to %d.\n", (int)(maxOutputSize_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] batchSize is supposed to be >= 1 but it is set to %d but you did not specify either --pretrain or --train therefore this can be ignored.\n", (int)(maxOutputSize_raw->valuedouble));
                }
            }
            else{
                if (do_pretrain || do_train){
                    batchSize = (int)(batchSize_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring batchSize as you did not specify either --pretrain or --train.\n");
                }
            }
        }
    }

    cJSON* antiOverfittingOptimisations_raw = cJSON_GetObjectItem(config, "antiOverfittingOptimisations");
    bool antiOverfittingOptimisations = false; //yea i'll be fair here i can't put a cool dummy value.
    
    if (!cJSON_IsBool(antiOverfittingOptimisations_raw)){
        if (do_pretrain || do_train){
            printf("[Config] [Fatal] antiOverfittingOptimisations is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] antiOverfittingOptimisations is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (do_pretrain || do_train){
            antiOverfittingOptimisations = cJSON_IsTrue(antiOverfittingOptimisations_raw);
        }
        else{
            printf("[Config] [Info] Ignoring antiOverfittingOptimisations as you did not specify either --pretrain or --train.\n");
        }
    }

    cJSON* autosave_raw = cJSON_GetObjectItem(config, "autosave");
    bool autosave = false;
    
    if (!cJSON_IsBool(autosave_raw)){
        if (do_pretrain || do_train){
            printf("[Config] [Fatal] autosave is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] autosave is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (do_pretrain || do_train){
            autosave = cJSON_IsTrue(autosave_raw);
        }
        else{
            printf("[Config] [Info] Ignoring autosave as you did not specify either --pretrain or --train.\n");
        }
    }

    cJSON* embeddingSize_raw = cJSON_GetObjectItem(config, "embeddingSize");
    if (!cJSON_IsNumber(embeddingSize_raw)){
        if (new){
            printf("[Config] [Fatal] embeddingSize is missing/corrutped.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] embeddingSize is missing/corrupted but you are loading a model, not creating one therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(embeddingSize_raw->valuedouble)){
            if (new){
                printf("[Config] [Fatal] embeddingSize is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] embeddingSize is supposed to be an int but it is a float, but you are loading a model, not creating a new one therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(embeddingSize_raw->valuedouble) < 1){
                if (new){
                    printf("[Config] [Fatal] embeddingSize is supposed to be >= 1 but it is set to %d.\n", (int)(embeddingSize_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] embeddingSize is supposed to be >= 1 but it is set to %d, but you are loading a model, not creating a new one therefore this can be ignored.\n");
                }
            }
            else{
                if (new){
                    embeddingSize = (int)(embeddingSize_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring embeddingSize as you are loading a model, not creating a new one.\n");
                }
            }
        }
    }

    cJSON* layersAmount_raw = cJSON_GetObjectItem(config, "layersAmount");
    int layersAmount = -1;
    if (!cJSON_IsNumber(layersAmount_raw)){
        if (new){
            printf("[Config] [Fatal] layersAmount is missing/corrutped.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] layersAmount is missing/corrupted but you are loading a model, not creating one therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(layersAmount_raw->valuedouble)){
            if (new){
                printf("[Config] [Fatal] layersAmount is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] layersAmount is supposed to be an int but it is a float, but you are loading a model, not creating a new one therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(layersAmount_raw->valuedouble) < 1){
                if (new){
                    printf("[Config] [Fatal] layersAmount is supposed to be >= 1 but it is set to %d.\n", (int)(embeddingSize_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] layersAmount is supposed to be >= 1 but it is set to %d, but you are loading a model, not creating a new one therefore this can be ignored.\n");
                }
            }
            else{
                if (new){
                    layersAmount = (int)(layersAmount_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring layersAmount as you are loading a model, not creating a new one.\n");
                }
            }
        }
    }

    cJSON* heads_raw = cJSON_GetObjectItem(config, "heads");
    if (!cJSON_IsNumber(heads_raw)){
        if (new){
            printf("[Config] [Fatal] heads is missing/corrutped.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] heads is missing/corrupted but you are loading a model, not creating one therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(heads_raw->valuedouble)){
            if (new){
                printf("[Config] [Fatal] heads is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] heads is supposed to be an int but it is a float, but you are loading a model, not creating a new one therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(heads_raw->valuedouble) < 1){
                if (new){
                    printf("[Config] [Fatal] heads is supposed to be >= 1 but it is set to %d.\n", (int)(embeddingSize_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] heads is supposed to be >= 1 but it is set to %d, but you are loading a model, not creating a new one therefore this can be ignored.\n");
                }
            }
            else{
                if (new){
                    heads = (int)(heads_raw->valuedouble);
                    if (embeddingSize % heads != 0){
                        printf("[Config] [Fatal] embeddingSize must be divisible by heads. Got embeddingSize=%d and heads=%d.\n", embeddingSize, heads);
                        return 1;
                    }
                    head_dim = embeddingSize / heads;
                }
                else{
                    printf("[Config] [Info] Ignoring heads as you are loading a model, not creating a new one.\n");
                }
            }
        }
    }

    cJSON* learningRateDecay_raw = cJSON_GetObjectItem(config, "learningRateDecay");
    double lr_reduce_amount = 0xdeadbeef;
    if (!cJSON_IsNumber(learningRateDecay_raw)){
        if (do_train || do_pretrain){
            printf("[Config] [Fatal] learningRateDecay is missing/corrutped.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] learningRateDecay is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (learningRateDecay_raw->valuedouble < 0){
            if ((!do_pretrain) && (!do_train)){
                printf("[Config] [Info] Ignoring learningRateDecay as you did not specify either --pretrain or --train.\n");
                printf("[Console] [Info] Showing tip anyways :)\n");
            }
            else{
                lr_reduce_amount = learningRateDecay_raw->valuedouble;
            }
            printf("[Config] [Tip] learningRateDecay is set to a negative number, this will cause learningRate to rise when patience is reached on plateau. This is not recomended, you are on your own, good luck.\n");
        }
        else{
            if ((!do_pretrain) && (!do_train)){
                printf("[Config] [Info] Ignoring learningRateDecay as you did not specify either --pretrain or --train.\n");
            }
            else{
                lr_reduce_amount = learningRateDecay_raw->valuedouble;
            }
        }
    }

    cJSON* learningRateDecayPatience_raw = cJSON_GetObjectItem(config, "learningRateDecayPatience");
    int patience = -1;

    if (!cJSON_IsNumber(learningRateDecayPatience_raw)){
        if (do_train || do_pretrain){
            printf("[Config] [Fatal] learningRateDecayPatience is missing/corrutped.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] learningRateDecayPatience is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(learningRateDecayPatience_raw->valuedouble)){
            if (do_train || do_pretrain){
                printf("[Config] [Fatal] learningRateDecayPatience is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] learningRateDecayPatience is supposed to be an int but it is a float, but you did not specify either --pretrain or --train therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(learningRateDecayPatience_raw->valuedouble) < 0){
                if (do_train || do_pretrain){
                    printf("[Config] [Fatal] learningRateDecayPatience is supposed to be >= 0 but it is set to %d.\n", (int)(learningRateDecayPatience_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] learningRateDecayPatience is supposed to be >= 0 but it is set to %d, but you did not specify either --pretrain or --train therefore this can be ignored.\n", (int)(learningRateDecayPatience_raw->valuedouble));
                }
            }
            else{
                if ((!do_pretrain) && (!do_train)){
                    printf("[Config] [Info] Ignoring learningRateDecayPatience as you did not specify either --pretrain or --train.\n");
                }
                else{
                    patience = (int)(learningRateDecayPatience_raw->valuedouble);
                }
            }
        }
    }

    cJSON* biasesinitrange_raw = cJSON_GetObjectItem(config, "biasesinitrange");
    float* biasesinitrange = NULL;

    if (!cJSON_IsArray(biasesinitrange_raw)){
        if (new){
            printf("[Config] [Fatal] biasesinitrange is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] biasesinitrange is missing/corrupted but you are loading a model, not creating a new one therefore this can be ignored.\n");
        }
    }
    else{
        int arr_size = cJSON_GetArraySize(biasesinitrange_raw);
        if (!(arr_size == 2)){
            if (new){
                printf("[Config] [Fatal] biasesinitrange is supposed to contain two elements, it currently contains %d elements.\n", arr_size);
                return 1;
            }
            else{
                printf("[Config] [Warning] biasesinitrange is supposed to contain two elements, it currently contains %d elements but you are loading a model, not creating a new one therefore this can be ignored.\n", arr_size);
            }
        }
        else{
            cJSON* elem_a = cJSON_GetArrayItem(biasesinitrange_raw, 0);
            cJSON* elem_b = cJSON_GetArrayItem(biasesinitrange_raw, 1);
            
            if (!cJSON_IsNumber(elem_a)){
                if (new){
                    printf("[Config] [Fatal] biasesinitrange's elements are supposed to be numbers, however first element is an impostor.\n");
                    return 1;
                }
                else{
                    printf("[Config] [Warning] biasesinitrange's elements are supposed to be numbers, however the first element is an impostor, but because you are loading a model and not creating a new one this can be ignored.\n");
                }
            }
            
            if (!cJSON_IsNumber(elem_b)){
                if (new){
                    printf("[Config] [Fatal] biasesinitrange's elements are supposed to be numbers, however second element is an impostor.\n");
                    return 1;
                }
                else{
                    printf("[Config] [Warning] biasesinitrange's elements are supposed to be numbers, however the second element is an impostor, but because you are loading a model and not creating a new one this can be ignored.\n");
                }
            }

            if (new){
                biasesinitrange = malloc(2 * sizeof(double));
                if (!biasesinitrange){
                    printf("Failed memory allocation to parse config.\n");
                    return 1;
                }
                biasesinitrange[0] = (float)(elem_a->valuedouble);
                biasesinitrange[1] = (float)(elem_b->valuedouble);
            }
            else{
                printf("[Config] [Info] Ignoring biasesinitrange as you are loading a model, not creating a new one.\n");
            }
        }
    }

    cJSON* embeddinginitrange_raw = cJSON_GetObjectItem(config, "embeddinginitrange");
    float* embeddinginitrange = NULL;

    if (!cJSON_IsArray(embeddinginitrange_raw)){
        if (new){
            printf("[Config] [Fatal] embeddinginitrange is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] embeddinginitrange is missing/corrupted but you are loading a model, not creating a new one therefore this can be ignored.\n");
        }
    }
    else{
        int arr_size = cJSON_GetArraySize(embeddinginitrange_raw);
        if (!(arr_size == 2)){
            if (new){
                printf("[Config] [Fatal] embeddinginitrange is supposed to contain two elements, it currently contains %d elements.\n", arr_size);
                return 1;
            }
            else{
                printf("[Config] [Warning] embeddinginitrange is supposed to contain two elements, it currently contains %d elements but you are loading a model, not creating a new one therefore this can be ignored.\n", arr_size);
            }
        }
        else{
            cJSON* elem_a = cJSON_GetArrayItem(embeddinginitrange_raw, 0);
            cJSON* elem_b = cJSON_GetArrayItem(embeddinginitrange_raw, 1);
            
            if (!cJSON_IsNumber(elem_a)){
                if (new){
                    printf("[Config] [Fatal] embeddinginitrange's elements are supposed to be numbers, however first element is an impostor.\n");
                    return 1;
                }
                else{
                    printf("[Config] [Warning] embeddinginitrange's elements are supposed to be numbers, however the first element is an impostor, but because you are loading a model and not creating a new one this can be ignored.\n");
                }
            }
            
            if (!cJSON_IsNumber(elem_b)){
                if (new){
                    printf("[Config] [Fatal] embeddinginitrange's elements are supposed to be numbers, however second element is an impostor.\n");
                    return 1;
                }
                else{
                    printf("[Config] [Warning] embeddinginitrange's elements are supposed to be numbers, however the second element is an impostor, but because you are loading a model and not creating a new one this can be ignored.\n");
                }
            }

            if (new){
                embeddinginitrange = malloc(2 * sizeof(double));
                if (!embeddinginitrange){
                    printf("Failed memory allocation to parse config.\n");
                    return 1;
                }
                embeddinginitrange[0] = (float)(elem_a->valuedouble);
                embeddinginitrange[1] = (float)(elem_b->valuedouble);
            }
            else{
                printf("[Config] [Info] Ignoring embeddinginitrange as you are loading a model, not creating a new one.\n");
            }
        }
    }

    cJSON_Delete(config);

after_config_parse:
    float* he_init(float fan_in){
        float* returns = malloc(2 * sizeof(float));
        if (!returns){
            printf("Failed memory allocation for weights initalisation range calculation.\n");
            return NULL;
        }
        float range = sqrtf(2.0f / fan_in);
        returns[0] = -range;
        returns[1] = range;
        return returns;
    }

    float* weightsinitrange = NULL;
    if (do_pretrain || do_train){
        printf("Calculating weight initalisation range with he init...\n");
        weightsinitrange = he_init(embeddingSize);
        if (!weightsinitrange){
            return 1;
        }
        printf("Calculated weight initalisation range with he init.\n");
    }

    printf("Reading vocabulary file (vocabulary.json)...\n");
    char* vocab_file = read_file("vocabulary.json");
    if (!vocab_file){
        printf("Failed to read vocabulary file.\n");
        return 1;
    }
    printf("Read vocabulary file.\n");
    printf("Parsing vocabulary...\n");
    cJSON* vocab = cJSON_Parse(vocab_file);
    if (!vocab){
        printf("Failed to parse vocabulary.\n");
        return 1;
    }
    free(vocab_file);
    if (!cJSON_IsArray(vocab)){
        printf("Vocabulary is corrutped.\n");
        return 1;
    }
    printf("Parsed vocabulary.\n");
    long long timer_ = timer();
    printf("Computing id to token table...\n");
    
    int vocab_len = cJSON_GetArraySize(vocab);
    int* vocab_per_toksize = NULL;
    int vocab_per_toksize_len = 0;

    cJSON* item = vocab->child;
    int index = 0;

    while (item != NULL){
        if (!cJSON_IsString(item)) {
            printf("Vocabulary item %d/%d is corrupted.\n", index + 1, vocab_len);
            return 1;
        }

        int item_strlen = strlen(item->valuestring) + 1; // +1 for null terminator

        // Grow vocab_per_toksize array
        vocab_per_toksize = realloc(vocab_per_toksize, (vocab_per_toksize_len + 1) * sizeof(int));
        if (!vocab_per_toksize) {
            printf("Failed to allocate memory for vocab_per_toksize.\n");
            return 1;
        }
        vocab_per_toksize[vocab_per_toksize_len++] = item_strlen;

        item = item->next;
        index++;
    }
    char** id_to_tok = malloc(vocab_len * sizeof(char*));
    if (!id_to_tok){
        printf("Failed memory allocation to compute id to token table.\n");
        return 1;
    }
    int vocab_index = 0;
    cJSON* item_outer = vocab->child;
    char* item_ = NULL;
    for (int index = 0; index < vocab_len; index++){
        item_ = item_outer->valuestring;
        id_to_tok[index] = malloc(vocab_per_toksize[vocab_index]);
        if (!id_to_tok[index]){
            printf("Failed memory allocation to compute id to token table.\n");
            return 1;
        }
        strcpy(id_to_tok[index], item_);
        item_outer = item_outer->next;
        vocab_index++;
    }
    printf("Computed id to token table in %lldms.\n", timer_end(timer_));
    printf("Computing token to id data...\n");
    timer_ = timer();

    typedef struct { //I have joined the dark side of structs.
        char* token;
        int id;
    } TokenEntry;

    TokenEntry* token_to_id_tokensort = malloc(vocab_len * sizeof(TokenEntry));

    for (int index = 0; index < vocab_len; index++){
        token_to_id_tokensort[index].token = id_to_tok[index];
        token_to_id_tokensort[index].id = index;
    }
    
    int cmp_tokens(const void* a, const void* b) {
        return strcmp(((TokenEntry*)a)->token, ((TokenEntry*)b)->token);
    }

    qsort(token_to_id_tokensort, vocab_len, sizeof(TokenEntry), cmp_tokens);
    
    free(vocab_per_toksize);
    printf("Computed token to id data in %lldms.\n", timer_end(timer_));

    int token_to_id(char* tok) { //binary search bruh
        int left = 0;
        int right = vocab_len - 1;

        while (left <= right) {
            int mid = (left + right) / 2;
            int cmp = strcmp(tok, token_to_id_tokensort[mid].token);

            if (cmp == 0) {
                return token_to_id_tokensort[mid].id;
            }

            if (cmp < 0) {
                right = mid - 1;
            } 
            else {
                left = mid + 1;
            }
        }

        return -1;
    }

    char* id_to_token(int id){
        if (id >= vocab_len){
            return NULL;
        }
        else{
            if (id < 0){
                return NULL;
            }
            else{
                char* res = id_to_tok[id];
                return res;
            }
        }
    }

    typedef struct {
        size_t seq_len;
        int* tokens;
        bool success;
    } tokenize_ret;

    void free_tokenize_ret(tokenize_ret rets){
        free(rets.tokens);
        return;
    }

    int max_token_len = 0;
    for (int index = 0; index < vocab_len; index++) {
        char* tok = id_to_tok[index];
        if (tok) {
            int tok_len = strlen(tok);
            if (tok_len > max_token_len) {
                max_token_len = tok_len;
            }
        }
    }

    int unk_token_id = token_to_id("<|unk|>");
    if (unk_token_id == -1) {
        printf("Warning: <|unk|> token not found in vocabulary. Unknown characters will cause tokenization to fail.\n");
    }

    tokenize_ret tokenize(char* str_, bool verbose) {
        tokenize_ret out = {0};

        out.success = true;
        out.seq_len = 0;
        out.tokens = NULL;

        size_t len = strlen(str_);
        if (len == 0) {
            return out;
        }

        char* str = malloc(len + 1);
        if (!str) {
            printf("Failed to allocate memory to tokenize text.\n");
            out.success = false;
            return out;
        }
        strcpy(str, str_);

        int* tokenized = NULL;
        size_t tokenized_len = 0;
        size_t consumed = 0;
        int last_percent_hundredths = -1;
        long long start_time = time_ms();

        while (consumed < len) {
            // Progress tracking - update every 0.01%
            int current_percent_hundredths = (int)((consumed * 10000) / len);
            if (current_percent_hundredths > last_percent_hundredths) {
                double percent = current_percent_hundredths / 100.0;
                long long elapsed = time_ms() - start_time;
                int elapsed_s = (int)((double)(elapsed) / 1000);
                if (verbose){
                    printf("\rTokenizing: %.2f%% (time elapsed: %ds)", percent, elapsed_s);
                    fflush(stdout);
                }
                last_percent_hundredths = current_percent_hundredths;
            }

            size_t remaining = len - consumed;
            size_t cursor_max = (remaining > max_token_len) ? max_token_len : remaining;
            int found = 0;

            for (size_t cursor = cursor_max; cursor > 0; cursor--) {
                char saved = str[consumed + cursor];
                str[consumed + cursor] = '\0';

                int tok_id = token_to_id(str + consumed);

                str[consumed + cursor] = saved;

                if (tok_id != -1) {
                    int* new_arr = realloc(tokenized, (tokenized_len + 1) * sizeof(int));
                    if (!new_arr) {
                        printf("Failed to allocate memory to tokenize text.\n");
                        free(tokenized);
                        free(str);
                        out.seq_len = 0;
                        out.tokens = NULL;
                        return out;
                    }

                    tokenized = new_arr;
                    tokenized[tokenized_len] = tok_id;
                    tokenized_len++;

                    consumed += cursor;
                    found = 1;
                    break;
                }
            }

            if (!found) {
                // No token found, use <|unk|> token and skip one byte
                if (unk_token_id != -1) {
                    int* new_arr = realloc(tokenized, (tokenized_len + 1) * sizeof(int));
                    if (!new_arr) {
                        printf("Failed to allocate memory to tokenize text.\n");
                        free(tokenized);
                        free(str);
                        out.seq_len = 0;
                        out.tokens = NULL;
                        return out;
                    }

                    tokenized = new_arr;
                    tokenized[tokenized_len] = unk_token_id;
                    tokenized_len++;
                    consumed += 1; // Skip one byte and continue
                } else {
                    // No <|unk|> token available, fail
                    free(tokenized);
                    free(str);
                    out.seq_len = 0;
                    out.tokens = NULL;
                    return out;
                }
            }
        }

        long long final_elapsed = time_ms() - start_time;
        int final_elapsed_s = (int)((double)(final_elapsed) / 1000);
        if (verbose){
            printf("\rTokenizing: 100.00%% (time elapsed: %ds)\n", final_elapsed_s);
            fflush(stdout);
        }

        free(str);

        out.seq_len = tokenized_len;
        out.tokens = tokenized;

        return out;
    }

    float temperature = 0.7;
    int step_num = 0;
    typedef struct{
        float beta1;
        float beta2;
        float epsilon;
        int t;
    } ap;
    ap adam_params;
    adam_params.beta1 = 0.9;
    adam_params.beta2 = 0.98;
    adam_params.epsilon = 1e-9;
    adam_params.t = 0;

    printf("Initalizing model...\n");
    long long timer___ = timer();
    printf("Initalizing layers...\n");
    timer_ = timer();

    float random_range(float* ran){
        float min = ran[0];
        float max = ran[1];
        return min + ((float)rand() / (float)RAND_MAX) * (max - min);
    }

    typedef struct {
        float* param;
        float* m;
        float* v;
    } param;

    bool alloc_param(param* p, size_t count){
        p->param = calloc(count, sizeof(float));
        p->m = calloc(count, sizeof(float));
        p->v = calloc(count, sizeof(float));
        if ((!p->param) || (!p->m) || (!p->v)){
            return false;
        }
        return true;
    }

    typedef struct {
        struct {
            param normalize_1;
            struct {
                struct {
                    param query;
                    param key;
                    param value;
                } *heads;
                param output;
            } attention;
            param normalize_2;
            struct {
                param grow;
                param gate;
                param shrink;
            } feed_forward;
        } weights;
        struct {
            param normalize_1;
            struct {
                struct {
                    param query;
                    param key;
                    param value;
                } *heads;
                param output;
            } attention;
            param normalize_2;
            struct {
                param grow;
                param gate;
                param shrink;
            } feed_forward;
        } biases;
    } layer;

    typedef struct {
        param weights;
        param biases;
    } vp;

    vp vocab_projection;

    void dprintf(const char *format, ...) {
        if (!debug) {
            return;
        }

        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }

    //Chat we cookin
    layer* layers = NULL;
    param* embeddings = NULL;
    if (new){
        layers = malloc(layersAmount * sizeof(layer));
        if (!layers){
            printf("Failed to allocate memory to initalize layers.\n");
            return 1;
        }
        for (int index = 0; index < layersAmount; index++){
            printf("Initalizing layer %d/%d...\n", index + 1, layersAmount);
            long long timer__ = timer();
            if (!alloc_param(&layers[index].weights.normalize_1, embeddingSize) ||
                !alloc_param(&layers[index].weights.normalize_2, embeddingSize) ||
                !alloc_param(&layers[index].biases.normalize_1, embeddingSize) ||
                !alloc_param(&layers[index].biases.normalize_2, embeddingSize)){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            layers[index].weights.attention.heads = malloc(heads * sizeof(*layers[index].weights.attention.heads));
            layers[index].biases.attention.heads = malloc(heads * sizeof(*layers[index].biases.attention.heads));
            if (!layers[index].weights.attention.heads || !layers[index].biases.attention.heads){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!alloc_param(&layers[index].weights.attention.output, embeddingSize * (head_dim * heads)) ||
                !alloc_param(&layers[index].biases.attention.output, embeddingSize)){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!alloc_param(&layers[index].weights.feed_forward.grow, embeddingSize * (embeddingSize * ffnGrowSize)) ||
                !alloc_param(&layers[index].weights.feed_forward.gate, embeddingSize * (embeddingSize * ffnGrowSize)) ||
                !alloc_param(&layers[index].weights.feed_forward.shrink, embeddingSize * (embeddingSize * ffnGrowSize)) ||
                !alloc_param(&layers[index].biases.feed_forward.grow, (embeddingSize * ffnGrowSize)) ||
                !alloc_param(&layers[index].biases.feed_forward.gate, (embeddingSize * ffnGrowSize)) ||
                !alloc_param(&layers[index].biases.feed_forward.shrink, embeddingSize)){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }

            for (int subindex = 0; subindex < embeddingSize; subindex++){
                layers[index].weights.normalize_1.param[subindex] = 1.0f; // RMSNorm gamma
                layers[index].weights.normalize_2.param[subindex] = 1.0f; // RMSNorm gamma
                layers[index].biases.normalize_1.param[subindex] = 0.0f;  // beta
                layers[index].biases.normalize_2.param[subindex] = 0.0f;  // beta
            }

            for (int subindex = 0; subindex < heads; subindex++){
                if (!alloc_param(&layers[index].weights.attention.heads[subindex].query, head_dim * embeddingSize) ||
                    !alloc_param(&layers[index].weights.attention.heads[subindex].key, head_dim * embeddingSize) ||
                    !alloc_param(&layers[index].weights.attention.heads[subindex].value, head_dim * embeddingSize) ||
                    !alloc_param(&layers[index].biases.attention.heads[subindex].query, head_dim) ||
                    !alloc_param(&layers[index].biases.attention.heads[subindex].key, head_dim) ||
                    !alloc_param(&layers[index].biases.attention.heads[subindex].value, head_dim)){
                    printf("Failed to allocate memory to initalize layers.\n");
                    return 1;
                }

                for (int subindex_ = 0; subindex_ < head_dim * embeddingSize; subindex_++){
                    layers[index].weights.attention.heads[subindex].query.param[subindex_] = random_range(weightsinitrange);
                    layers[index].weights.attention.heads[subindex].key.param[subindex_] = random_range(weightsinitrange);
                    layers[index].weights.attention.heads[subindex].value.param[subindex_] = random_range(weightsinitrange);
                }

                for (int subindex_ = 0; subindex_ < head_dim; subindex_++){
                    layers[index].biases.attention.heads[subindex].query.param[subindex_] = random_range(biasesinitrange);
                    layers[index].biases.attention.heads[subindex].key.param[subindex_] = random_range(biasesinitrange);
                    layers[index].biases.attention.heads[subindex].value.param[subindex_] = random_range(biasesinitrange);
                }
            }

            for (int subindex = 0; subindex < embeddingSize * (head_dim * heads); subindex++){
                layers[index].weights.attention.output.param[subindex] = random_range(weightsinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize; subindex++){
                layers[index].biases.attention.output.param[subindex] = random_range(biasesinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize * (embeddingSize * ffnGrowSize); subindex++){
                layers[index].weights.feed_forward.grow.param[subindex] = random_range(weightsinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize * (embeddingSize * ffnGrowSize); subindex++){
                layers[index].weights.feed_forward.gate.param[subindex] = random_range(weightsinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize * ffnGrowSize; subindex++){
                layers[index].biases.feed_forward.grow.param[subindex] = random_range(biasesinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize * ffnGrowSize; subindex++){
                layers[index].biases.feed_forward.gate.param[subindex] = random_range(biasesinitrange);
            }

            for (int subindex = 0; subindex < (embeddingSize * ffnGrowSize) * embeddingSize; subindex++){
                layers[index].weights.feed_forward.shrink.param[subindex] = random_range(weightsinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize; subindex++){
                layers[index].biases.feed_forward.shrink.param[subindex] = random_range(biasesinitrange);
            }

            printf("Initalized layer %d/%d in %lldms.\n", index + 1, layersAmount, timer_end(timer__));
        }

        printf("Initalized layers in %lldms\n", timer_end(timer_));

        printf("Initalizing embeddings...\n");
        timer_ = timer();
        embeddings = malloc((vocab_len) * sizeof(param));
        if (!embeddings){
            printf("Failed to allocate memory to initalize embeddings.\n");
            return 1;
        }

        for (int index = 0; index < vocab_len; index++){
            if (!alloc_param(&embeddings[index], embeddingSize)){
                printf("Failed to allocate memory to initalize embeddings.\n");
                return 1;
            }
            for (int subindex = 0; subindex < embeddingSize; subindex++){
                embeddings[index].param[subindex] = random_range(embeddinginitrange);
            }
        }

        printf("Initalized embeddings in %lldms.\n", timer_end(timer_));
        
        printf("Initalizing vocabulary projection weights and biases.\n");
        timer_ = timer();
        
        if (!alloc_param(&vocab_projection.weights, vocab_len * embeddingSize) ||
            !alloc_param(&vocab_projection.biases, vocab_len)){
            printf("Failed memory allocation to initalize vocabulary projection.\n");
            return 1;
        }
        
        for (int index = 0; index < vocab_len * embeddingSize; index++){
            vocab_projection.weights.param[index] = random_range(weightsinitrange);
        }

        for (int index = 0; index < vocab_len; index++){
            vocab_projection.biases.param[index] = random_range(biasesinitrange);
        }

        printf("Initalized vocabulary projection weights and biases in %lldms.\n", timer_end(timer_));
        printf("Initalized model in %lldms.\n", timer_end(timer___));
    }
    else{
        if (load){
            printf("Opening model file (\"%s\").\n", model_location);
            long long timer_ = timer();
            mz_zip_archive zipfile;
            memset(&zipfile, 0, sizeof(zipfile));
            if (!mz_zip_reader_init_file(&zipfile, model_location, 0)){
                printf("Failed to open model file (\"%s\").\n", model_location);
                return 1;
            }
            printf("Opened model file in %lldms.\n", timer_end(timer_));

            int n_files = (int)(mz_zip_reader_get_num_files(&zipfile));
            printf("Loading model...\n");
            timer_ = timer();
            char*** files = malloc(n_files * sizeof(char**));
            size_t* files_len = malloc(n_files * sizeof(size_t));
            if (!files_len){
                printf("Failed to allocate memory to load model.\n");
                return 1;
            }
            if (!files){
                printf("Failed to allocate memory to load model.\n");
                return 1;
            }
            for (int index = 0; index < n_files; index++){
                printf("\rLoading model file %d/%d...", index + 1, n_files);
                fflush(stdout);
                files[index] = malloc(2 * sizeof(char*));
                if (!files[index]){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
                mz_zip_archive_file_stat file_info;
                if (mz_zip_reader_file_stat(&zipfile, index, &file_info)) {
                    if ((size_t)(file_info.m_uncomp_size) == 0){
                        printf("Corrupted model file.\n");
                        return 1;
                    }
                    int filename_len = strlen(file_info.m_filename) + 1;
                    files[index][0] = malloc(filename_len);
                    files[index][1] = malloc((size_t)(file_info.m_uncomp_size));
                    files_len[index] = (size_t)(file_info.m_uncomp_size);
                }
                else{
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_0\n");
                    return 1;
                }
                if (!files[index][0]){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
                if (!files[index][1]){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
                
                strcpy(files[index][0], file_info.m_filename);
                if (!mz_zip_reader_extract_to_mem(&zipfile, index, files[index][1], (size_t)(file_info.m_uncomp_size), 0)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_1\n");
                    return 1;
                }

                fflush(stdout);
            }
            printf("\nLoaded all model files.\n");

            mz_zip_reader_end(&zipfile);

            typedef struct {
                char* name;
                int index;
            } FileEntry;

            int cmp_files(const void *a, const void *b) {
                return strcmp(((const FileEntry*)a)->name, ((const FileEntry*)b)->name);
            }

            FileEntry *file_to_idx_sorted = malloc(n_files * sizeof(FileEntry));
            for (int index = 0; index < n_files; index++) {
                file_to_idx_sorted[index].name = files[index][0];
                file_to_idx_sorted[index].index  = index;
            }
            qsort(file_to_idx_sorted, n_files, sizeof(FileEntry), cmp_files);

            int filename_to_index(char *name) {
                int left = 0;
                int right = n_files - 1;
                while (left <= right) {
                    int middle = (left + right) / 2;
                    int cmp = strcmp(name, file_to_idx_sorted[middle].name);
                    if (cmp == 0){
                        return file_to_idx_sorted[middle].index;
                    }
                    if (cmp < 0){
                        right = middle - 1;
                    }
                    else{
                        left = middle + 1;
                    }
                }
                return -1;
            }

            bool found_model_meta = false;
            int model_meta_index = filename_to_index("model_meta.json");
            
            if (model_meta_index != -1){
                found_model_meta = true;
                
                char* model_meta = realloc(files[model_meta_index][1], files_len[model_meta_index] + 1);
                if (!model_meta){
                    printf("Failed memory allocation to load model.\n");
                    return 1; //exit, os will reclaim mem
                }
                model_meta[files_len[model_meta_index]] = '\0';
                files[model_meta_index][1] = model_meta;
                files_len[model_meta_index]++;
                
                if (strlen(model_meta) == 0){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_2\n");
                    return 1;
                }
            }

            if (!found_model_meta){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_3\n");
                return 1;
            }

            //if (debug){
            //    dprintf("Model meta:\n\"");
            //    for (int index = 0; index < files_len[model_meta_index]; index++){
            //        dprintf("%c", files[model_meta_index][1][index]);
            //    }
            //    dprintf("\"\n");
            //}
            cJSON* model_meta = cJSON_Parse(files[model_meta_index][1]);
            if (!model_meta){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_4\n");
                dprintf("cJSON additional info: \"%s\"\n", cJSON_GetErrorPtr());
                dprintf("First few bytes: '%.20s'\n", files[model_meta_index][1]);
                dprintf("Length: %zu\n", strlen(files[model_meta_index][1]));
                return 1;
            }
            if (!cJSON_IsObject(model_meta)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_5\n");
                return 1;
            }

            embeddingSize_raw = cJSON_GetObjectItem(model_meta, "embeddingSize");
            if (!cJSON_IsNumber(embeddingSize_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_6\n");
                return 1;
            }
            if (!isInt(embeddingSize_raw->valuedouble)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_7\n");
                return 1;
            }
            if ((int)(embeddingSize_raw->valuedouble) < 1){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_8\n");
                return 1;
            }
            embeddingSize = (int)(embeddingSize_raw->valuedouble);

            layersAmount_raw = cJSON_GetObjectItem(model_meta, "layersAmount");
            if (!cJSON_IsNumber(layersAmount_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_9\n");
                return 1;
            }
            if (!isInt(layersAmount_raw->valuedouble)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_10\n");
                return 1;
            }
            if ((int)(layersAmount_raw->valuedouble) < 1){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_11\n");
                return 1;
            }
            layersAmount = (int)(layersAmount_raw->valuedouble);

            heads_raw = cJSON_GetObjectItem(model_meta, "heads");
            if (!cJSON_IsNumber(heads_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_12\n");
                return 1;
            }
            if (!isInt(heads_raw->valuedouble)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_13\n");
                return 1;
            }
            if ((int)(heads_raw->valuedouble) < 1){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_14\n");
                return 1;
            }
            heads = (int)(heads_raw->valuedouble);
            if (embeddingSize % heads != 0){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_14a\n");
                return 1;
            }
            head_dim = embeddingSize / heads;

            cJSON* ffn_grow_size_meta = cJSON_GetObjectItem(model_meta, "ffnGrowSize");
            if (!cJSON_IsNumber(ffn_grow_size_meta)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_14b\n");
                return 1;
            }
            if (!isInt(ffn_grow_size_meta->valuedouble) || (int)(ffn_grow_size_meta->valuedouble) < 1){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_14c\n");
                return 1;
            }
            ffnGrowSize = (int)(ffn_grow_size_meta->valuedouble);

            biasesinitrange_raw = cJSON_GetObjectItem(model_meta, "biasesinitrange");
            if (!cJSON_IsArray(biasesinitrange_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_15\n");
                return 1;
            }
            if (cJSON_GetArraySize(biasesinitrange_raw) != 2){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_16\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetArrayItem(biasesinitrange_raw, 0))){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_17\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetArrayItem(biasesinitrange_raw, 1))){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_18\n");
                return 1;
            }
            if (!biasesinitrange){
                biasesinitrange = malloc(2 * sizeof(float));
                if (!biasesinitrange){
                    printf("Failed to allocate memory to load model.\n");
                    dprintf("Err code: 0_19\n");
                    return 1;
                }
            }
            biasesinitrange[0] = (float)(cJSON_GetArrayItem(biasesinitrange_raw, 0)->valuedouble);
            biasesinitrange[1] = (float)(cJSON_GetArrayItem(biasesinitrange_raw, 1)->valuedouble);
            
            embeddinginitrange_raw = cJSON_GetObjectItem(model_meta, "embeddinginitrange");
            if (!cJSON_IsArray(embeddinginitrange_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_20\n");
                return 1;
            }
            if (cJSON_GetArraySize(embeddinginitrange_raw) != 2){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_21\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetArrayItem(embeddinginitrange_raw, 0))){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_22\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetArrayItem(embeddinginitrange_raw, 1))){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_23\n");
                return 1;
            }
            if (!embeddinginitrange){
                embeddinginitrange = malloc(2 * sizeof(float));
                if (!embeddinginitrange){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
            }
            embeddinginitrange[0] = (float)(cJSON_GetArrayItem(embeddinginitrange_raw, 0)->valuedouble);
            embeddinginitrange[1] = (float)(cJSON_GetArrayItem(embeddinginitrange_raw, 1)->valuedouble);
            
            cJSON* adam_params_raw = cJSON_GetObjectItem(model_meta, "adam_params");
            if (!cJSON_IsObject(adam_params_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_24\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetObjectItem(adam_params_raw, "beta1"))){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_25\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetObjectItem(adam_params_raw, "beta2"))){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_26\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetObjectItem(adam_params_raw, "epsilon"))){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_27\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetObjectItem(adam_params_raw, "t"))){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_28\n");
                return 1;
            }
            if (!isInt(cJSON_GetObjectItem(adam_params_raw, "t")->valuedouble)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_29\n");
                return 1;
            }
            if ((int)(cJSON_GetObjectItem(adam_params_raw, "t")->valuedouble) < 0){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_30\n");
                return 1;
            }
            adam_params.beta1 = (float)(cJSON_GetObjectItem(adam_params_raw, "beta1")->valuedouble);
            adam_params.beta2 = (float)(cJSON_GetObjectItem(adam_params_raw, "beta2")->valuedouble);
            adam_params.epsilon = (float)(cJSON_GetObjectItem(adam_params_raw, "epsilon")->valuedouble);
            adam_params.t = (int)(cJSON_GetObjectItem(adam_params_raw, "t")->valuedouble);
            
            cJSON* step_num_raw = cJSON_GetObjectItem(model_meta, "step_num");
            if (!cJSON_IsNumber(step_num_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_31\n");
                return 1;
            }
            if (!isInt(step_num_raw->valuedouble)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_32\n");
                return 1;
            }
            if ((int)(step_num_raw->valuedouble) < 0){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_33\n");
                return 1;
            }
            step_num = (int)(step_num_raw->valuedouble);

            cJSON* transformer_structure = cJSON_GetObjectItem(model_meta, "transformer_structure");
            if (!cJSON_IsObject(transformer_structure)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_34\n");
                return 1;
            }
            cJSON* layers_raw = cJSON_GetObjectItem(transformer_structure, "layers");
            if (!cJSON_IsArray(layers_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_35\n");
                return 1;
            }

            if (layersAmount != cJSON_GetArraySize(layers_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_36\n");
                return 1;
            }

            bool load_moments = !(load && (!do_pretrain) && (!do_train));

            param load_param_blob(cJSON* patharr, size_t expected_count){
                param p = {0};
                if (!cJSON_IsArray(patharr)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_37\n");
                    exit(1);
                }
                if (cJSON_GetArraySize(patharr) != 1){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_38\n");
                    exit(1);
                }
                cJSON* item = cJSON_GetArrayItem(patharr, 0);
                if (!cJSON_IsString(item)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_39\n");
                    exit(1);
                }
                const char* fname = item->valuestring;
                int file_index = filename_to_index((char*)fname);
                if (file_index == -1){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_40\n");
                    exit(1);
                }

                size_t param_bytes = expected_count * sizeof(float);
                size_t expected_bytes_full = expected_count * 3 * sizeof(float);
                size_t actual_bytes = files_len[file_index];
                bool has_moments_blob = (actual_bytes == expected_bytes_full);
                if (!(actual_bytes == param_bytes || has_moments_blob)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_40a\n");
                    dprintf("File %s size %zu, expected %zu or %zu (count=%zu)\n", fname, actual_bytes, param_bytes, expected_bytes_full, expected_count);
                    exit(1);
                }

                float* blob = (float*)files[file_index][1];
                p.param = malloc(expected_count * sizeof(float));
                if (!p.param){
                    printf("Failed to allocate memory to load model.\n");
                    exit(1);
                }

                memcpy(p.param, blob, expected_count * sizeof(float));
                if (load_moments && has_moments_blob){
                    p.m = malloc(expected_count * sizeof(float));
                    p.v = malloc(expected_count * sizeof(float));
                    if ((!p.m) || (!p.v)){
                        printf("Failed to allocate memory to load model.\n");
                        exit(1);
                    }
                    memcpy(p.m, blob + expected_count, expected_count * sizeof(float));
                    memcpy(p.v, blob + expected_count * 2, expected_count * sizeof(float));
                }
                else{
                    p.m = NULL;
                    p.v = NULL;
                }

                return p;
            }

            layers = malloc(layersAmount * sizeof(layer));
            if (!layers){
                printf("Failed memory allocation to load model.\n");
                return 1;
            }

            for (int index = 0; index < cJSON_GetArraySize(layers_raw); index++){
                cJSON* layer_curr = cJSON_GetArrayItem(layers_raw, index);
                if (!cJSON_IsObject(layer_curr)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_41\n");
                    return 1;
                }
                cJSON* weights_lc = cJSON_GetObjectItem(layer_curr, "weights");
                if (!cJSON_IsObject(weights_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_42\n");
                    return 1;
                }
                cJSON* normalize_1_lc = cJSON_GetObjectItem(weights_lc, "normalize_1");
                if (!cJSON_IsArray(normalize_1_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_43\n");
                    return 1;
                }
                layers[index].weights.normalize_1 = load_param_blob(normalize_1_lc, embeddingSize);
                
                cJSON* normalize_2_lc = cJSON_GetObjectItem(weights_lc, "normalize_2");
                if (!cJSON_IsArray(normalize_2_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_44\n");
                    return 1;
                }
                layers[index].weights.normalize_2 = load_param_blob(normalize_2_lc, embeddingSize);

                cJSON* attention_lc = cJSON_GetObjectItem(weights_lc, "attention");
                if (!cJSON_IsObject(attention_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_45\n");
                    return 1;
                }

                cJSON* heads_lc = cJSON_GetObjectItem(attention_lc, "heads");
                if (!cJSON_IsArray(heads_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_46\n");
                    return 1;
                }

                if (cJSON_GetArraySize(heads_lc) != heads){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_47\n");
                    return 1;
                }

                layers[index].weights.attention.heads = malloc(heads * sizeof(*layers[index].weights.attention.heads));
                if (!layers[index].weights.attention.heads){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }

                for (int subindex = 0; subindex < heads; subindex++){
                    if (!cJSON_IsObject(cJSON_GetArrayItem(heads_lc, subindex))){
                        printf("Model file is corrupted.\n");
                        dprintf("Err code: 0_48\n");
                        return 1;
                    }
                    cJSON* query_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "query");
                    cJSON* key_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "key");
                    cJSON* value_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "value");
                    if (!cJSON_IsArray(query_lh_lc)){
                        printf("Model file is corrupted.\n");
                        dprintf("Err code: 0_49\n");
                        return 1;
                    }
                    if (!cJSON_IsArray(key_lh_lc)){
                        printf("Model file is corrupted.\n");
                        dprintf("Err code: 0_50\n");
                        return 1;
                    }
                    if (!cJSON_IsArray(value_lh_lc)){
                        printf("Model file is corrupted.\n");
                        dprintf("Err code: 0_51\n");
                        return 1;
                    }
                    layers[index].weights.attention.heads[subindex].query = load_param_blob(query_lh_lc, head_dim * embeddingSize);
                    layers[index].weights.attention.heads[subindex].key = load_param_blob(key_lh_lc, head_dim * embeddingSize);
                    layers[index].weights.attention.heads[subindex].value = load_param_blob(value_lh_lc, head_dim * embeddingSize);
                }
                
                cJSON* attn_o_lc = cJSON_GetObjectItem(attention_lc, "output");
                if (!cJSON_IsArray(attn_o_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_52\n");
                    return 1;
                }
                layers[index].weights.attention.output = load_param_blob(attn_o_lc, embeddingSize * (head_dim * heads));

                cJSON* ffw_lc = cJSON_GetObjectItem(weights_lc, "feed_forward");
                if (!cJSON_IsObject(ffw_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_53\n");
                    return 1;
                }

                cJSON* ffw_grow_lc = cJSON_GetObjectItem(ffw_lc, "grow");
                cJSON* ffw_gate_lc = cJSON_GetObjectItem(ffw_lc, "gate");
                cJSON* ffw_shrink_lc = cJSON_GetObjectItem(ffw_lc, "shrink");
                if (!cJSON_IsArray(ffw_grow_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_54\n");
                    return 1;
                }
                if (!cJSON_IsArray(ffw_gate_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_54a\n");
                    return 1;
                }
                if (!cJSON_IsArray(ffw_shrink_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_55\n");
                    return 1;
                }
                layers[index].weights.feed_forward.grow = load_param_blob(ffw_grow_lc, embeddingSize * (embeddingSize * ffnGrowSize));
                layers[index].weights.feed_forward.gate = load_param_blob(ffw_gate_lc, embeddingSize * (embeddingSize * ffnGrowSize));
                layers[index].weights.feed_forward.shrink = load_param_blob(ffw_shrink_lc, embeddingSize * (embeddingSize * ffnGrowSize));


                weights_lc = cJSON_GetObjectItem(layer_curr, "biases");
                if (!cJSON_IsObject(weights_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_56\n");
                    return 1;
                }

                cJSON* normalize_1_blc = cJSON_GetObjectItem(weights_lc, "normalize_1");
                cJSON* normalize_2_blc = cJSON_GetObjectItem(weights_lc, "normalize_2");
                if (!cJSON_IsArray(normalize_1_blc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_56a\n");
                    return 1;
                }
                if (!cJSON_IsArray(normalize_2_blc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_56b\n");
                    return 1;
                }
                layers[index].biases.normalize_1 = load_param_blob(normalize_1_blc, embeddingSize);
                layers[index].biases.normalize_2 = load_param_blob(normalize_2_blc, embeddingSize);

                attention_lc = cJSON_GetObjectItem(weights_lc, "attention");
                if (!cJSON_IsObject(attention_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_59\n");
                    return 1;
                }

                heads_lc = cJSON_GetObjectItem(attention_lc, "heads");
                if (!cJSON_IsArray(heads_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_60\n");
                    return 1;
                }

                if (cJSON_GetArraySize(heads_lc) != heads){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_61\n");
                    return 1;
                }

                layers[index].biases.attention.heads = malloc(heads * sizeof(*layers[index].biases.attention.heads));
                if (!layers[index].biases.attention.heads){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }

                for (int subindex = 0; subindex < heads; subindex++){
                    if (!cJSON_IsObject(cJSON_GetArrayItem(heads_lc, subindex))){
                        printf("Model file is corrupted.\n");
                        dprintf("Err code: 0_62\n");
                        return 1;
                    }
                    cJSON* query_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "query");
                    cJSON* key_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "key");
                    cJSON* value_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "value");
                    if (!cJSON_IsArray(query_lh_lc)){
                        printf("Model file is corrupted.\n");
                        dprintf("Err code: 0_63\n");
                        return 1;
                    }
                    if (!cJSON_IsArray(key_lh_lc)){
                        printf("Model file is corrupted.\n");
                        dprintf("Err code: 0_64\n");
                        return 1;
                    }
                    if (!cJSON_IsArray(value_lh_lc)){
                        printf("Model file is corrupted.\n");
                        dprintf("Err code: 0_65\n");
                        return 1;
                    }
                    layers[index].biases.attention.heads[subindex].query = load_param_blob(query_lh_lc, head_dim);
                    layers[index].biases.attention.heads[subindex].key = load_param_blob(key_lh_lc, head_dim);
                    layers[index].biases.attention.heads[subindex].value = load_param_blob(value_lh_lc, head_dim);
                }

                attn_o_lc = cJSON_GetObjectItem(attention_lc, "output");
                if (!cJSON_IsArray(attn_o_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_66\n");
                    return 1;
                }
                layers[index].biases.attention.output = load_param_blob(attn_o_lc, embeddingSize);

                ffw_lc = cJSON_GetObjectItem(weights_lc, "feed_forward");
                if (!cJSON_IsObject(ffw_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_67\n");
                    return 1;
                }

                ffw_grow_lc = cJSON_GetObjectItem(ffw_lc, "grow");
                ffw_gate_lc = cJSON_GetObjectItem(ffw_lc, "gate");
                ffw_shrink_lc = cJSON_GetObjectItem(ffw_lc, "shrink");
                if (!cJSON_IsArray(ffw_grow_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_68\n");
                    return 1;
                }
                if (!cJSON_IsArray(ffw_gate_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_68a\n");
                    return 1;
                }
                if (!cJSON_IsArray(ffw_shrink_lc)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_69\n");
                    return 1;
                }
                layers[index].biases.feed_forward.grow = load_param_blob(ffw_grow_lc, embeddingSize * ffnGrowSize);
                layers[index].biases.feed_forward.gate = load_param_blob(ffw_gate_lc, embeddingSize * ffnGrowSize);
                layers[index].biases.feed_forward.shrink = load_param_blob(ffw_shrink_lc, embeddingSize);
            }
            cJSON* embeddings_raw = cJSON_GetObjectItem(transformer_structure, "embeddings");
            if (!cJSON_IsArray(embeddings_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_70\n");
                return 1;
            }
            int embeddings_raw_size = cJSON_GetArraySize(embeddings_raw);
            if (embeddings_raw_size != vocab_len){
                printf("The model you are trying to load doesn't use the same vocabulary as yours.\n");
                return 1;
            }
            cJSON* curr_embedding_raw_item = cJSON_GetArrayItem(embeddings_raw, 0);

            embeddings = calloc(vocab_len, sizeof(param));
            if (!embeddings){
                printf("Failed to allocate memory to load model.\n");
                return 1;
            }

            for (int index = 0; index < embeddings_raw_size; index++){
                if (!cJSON_IsArray(curr_embedding_raw_item)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_71\n");
                    return 1;
                }
                embeddings[index] = load_param_blob(curr_embedding_raw_item, embeddingSize);
                curr_embedding_raw_item = curr_embedding_raw_item->next;
            }

            cJSON* vocab_projection_raw = cJSON_GetObjectItem(transformer_structure, "vocab_projection");
            if (!cJSON_IsObject(vocab_projection_raw)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_75\n");
                return 1;
            }
            cJSON* vocab_projection_raw_weights = cJSON_GetObjectItem(vocab_projection_raw, "weights");
            cJSON* vocab_projection_raw_biases = cJSON_GetObjectItem(vocab_projection_raw, "biases");
            if (!cJSON_IsArray(vocab_projection_raw_weights)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_76\n");
                return 1;
            }
            if (!cJSON_IsArray(vocab_projection_raw_biases)){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_77\n");
                return 1;
            }
            
            int vocab_projection_raw_weights_len = cJSON_GetArraySize(vocab_projection_raw_weights);
            int vocab_projection_raw_biases_len = cJSON_GetArraySize(vocab_projection_raw_biases);

            if (vocab_projection_raw_weights_len != vocab_len){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_76a\n");
                return 1;
            }
            if (vocab_projection_raw_biases_len != vocab_len){
                printf("Model file is corrupted.\n");
                dprintf("Err code: 0_77a\n");
                return 1;
            }

            if (!alloc_param(&vocab_projection.weights, vocab_len * embeddingSize) ||
                !alloc_param(&vocab_projection.biases, vocab_len)){
                printf("Failed memory allocation to load model.\n");
                return 1;
            }

            cJSON* curr = vocab_projection_raw_weights->child;
            int wi = 0;
            for (int index = 0; index < vocab_projection_raw_weights_len; index++, curr = curr->next){
                if (!curr || !cJSON_IsArray(curr)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_78\n");
                    return 1;
                }
                param chunk = load_param_blob(curr, embeddingSize);
                memcpy(&vocab_projection.weights.param[wi], chunk.param, embeddingSize * sizeof(float));
                if (chunk.m){
                    memcpy(&vocab_projection.weights.m[wi], chunk.m, embeddingSize * sizeof(float));
                }
                if (chunk.v){
                    memcpy(&vocab_projection.weights.v[wi], chunk.v, embeddingSize * sizeof(float));
                }
                wi += embeddingSize;
                free(chunk.param);
                free(chunk.m);
                free(chunk.v);
            }

            curr = vocab_projection_raw_biases->child;
            int bi = 0;
            for (int index = 0; index < vocab_projection_raw_biases_len; index++, curr = curr->next){
                if (!curr || !cJSON_IsArray(curr)){
                    printf("Model file is corrupted.\n");
                    dprintf("Err code: 0_82\n");
                    return 1;
                }
                param chunk = load_param_blob(curr, 1);
                vocab_projection.biases.param[bi] = chunk.param[0];
                if (chunk.m){
                    vocab_projection.biases.m[bi] = chunk.m[0];
                }
                if (chunk.v){
                    vocab_projection.biases.v[bi] = chunk.v[0];
                }
                bi += 1;
                free(chunk.param);
                free(chunk.m);
                free(chunk.v);
            }

            for (int index = 0; index < n_files; index++){
                if (files[index][0]){
                    free(files[index][0]);
                }
                if (files[index][1]){
                    free(files[index][1]);
                }
                free(files[index]);
            }
            free(files);
            free(file_to_idx_sorted);
            free(files_len);
            cJSON_Delete(model_meta);
            printf("Loaded model in %lldms.\n", timer_end(timer_));
        }
    }

    size_t param_total = (size_t)(
        vocab_len*embeddingSize + vocab_len +
        layersAmount * (
            4*embeddingSize +
            heads*3*(head_dim*embeddingSize + head_dim) +
            embeddingSize*head_dim*heads + embeddingSize +
            3*embeddingSize*embeddingSize*ffnGrowSize +
            2*embeddingSize*ffnGrowSize + embeddingSize
        )
    );
    
    double total_mb_memory = 0;
    size_t real_param_total = 0;

    //To compute memory usage, compute using heaviest optimizer
    char* optimizer = NULL;
    if ((!train_optimizer) && (!pre_train_optimizer)){
        optimizer = "sgd";
        goto after_opt_select;
    }

    if (train_optimizer && strcmp(train_optimizer, "adam") == 0){
        optimizer = "adam";
    }
    else if (pre_train_optimizer && strcmp(pre_train_optimizer, "adam") == 0){
        optimizer = "adam";
    }
    else if (train_optimizer && strcmp(train_optimizer, "sgd_momentum") == 0){
        optimizer = "sgd_momentum";
    }
    else if (pre_train_optimizer && strcmp(pre_train_optimizer, "sgd_momentum") == 0){
        optimizer = "sgd_momentum";
    }
    else if (train_optimizer && strcmp(train_optimizer, "sgd") == 0){
        optimizer = "sgd";
    }
    else if (pre_train_optimizer && strcmp(pre_train_optimizer, "sgd") == 0){
        optimizer = "sgd";
    }
    else {
        optimizer = "sgd"; // fallback
    }

after_opt_select:
    printf("\nTotal model parameters and optimizer parameters:\n");
    printf("  %zu (~%.2fM) model parameters (~%.2fmb memory)\n", param_total, (double)(param_total) / 1000000, (double)(param_total) * 4 / 1024 / 1024);
    total_mb_memory += (double)(param_total) * 4 / 1024 / 1024;
    real_param_total += param_total;
    if (strcmp(optimizer, "sgd") == 0){
        printf("  0 (~0.00M) moments optimizer parameters (~0.00mb memory)\n");
    }
    else{
        if ((strcmp(optimizer, "sgd_momentum") == 0) || (strcmp(optimizer, "adam") == 0)){
            if (do_pretrain || do_train){
                printf("  %zu (~%.2fM) moments optimizer parameters (~%.2fmb memory)\n", param_total, (double)(param_total) / 1000000, (double)(param_total) * 4 / 1024 / 1024);
                total_mb_memory += (double)(param_total) * 4 / 1024 / 1024;
                real_param_total += param_total;
            }
        }
    }
    if ((strcmp(optimizer, "sgd") == 0) || (strcmp(optimizer, "sgd_momentum") == 0)){
        printf("  0 (~0.00M) varience optimizer parameters (~0.00mb memory)\n");
    }
    else{
        if (strcmp(optimizer, "adam") == 0){
            if (do_pretrain || do_train){
                printf("  %zu (~%.2fM) varience optimizer parameters (~%.2fmb memory)\n", param_total, (double)(param_total) / 1000000, (double)(param_total) * 4 / 1024 / 1024);
                total_mb_memory += (double)(param_total) * 4 / 1024 / 1024;
                real_param_total += param_total;
            }
        }
    }
    printf("Total:\n");
    printf("  %zu (%.2fM) model and optimizer parameters.\n", real_param_total, (double)(real_param_total) / 1000000);
    printf("  %.2fmb memory usage.\n", total_mb_memory);

    bool save(char* filepath){
        if (!filepath){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return false;
        }

        printf("Saving model at path \"%s\"...\n", filepath);
        long long save_timer = timer();

        //I tought the following function existed but it didn't, let's satisfy the code.
        void cJSON_AddNumberToArray(cJSON* arr, double n){
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(n));
            return;
        }

        cJSON* model_meta_root = cJSON_CreateObject();
        
        cJSON_AddNumberToObject(model_meta_root, "contextSize", contextSize);
        cJSON_AddNumberToObject(model_meta_root, "embeddingSize", embeddingSize);
        cJSON_AddNumberToObject(model_meta_root, "learningRate", learningRate);
        cJSON_AddNumberToObject(model_meta_root, "maxOutputSize", maxOutputSize);
        cJSON_AddNumberToObject(model_meta_root, "layersAmount", layersAmount);
        cJSON_AddNumberToObject(model_meta_root, "heads", heads);
        cJSON_AddNumberToObject(model_meta_root, "ffnGrowSize", ffnGrowSize);
        
        cJSON* biasesinitrange_save = cJSON_CreateArray();
        cJSON_AddNumberToArray(biasesinitrange_save, biasesinitrange[0]);
        cJSON_AddNumberToArray(biasesinitrange_save, biasesinitrange[1]);
        cJSON_AddItemToObject(model_meta_root, "biasesinitrange", biasesinitrange_save);

        cJSON* embeddinginitrange_save = cJSON_CreateArray();
        cJSON_AddNumberToArray(embeddinginitrange_save, embeddinginitrange[0]);
        cJSON_AddNumberToArray(embeddinginitrange_save, embeddinginitrange[1]);
        cJSON_AddItemToObject(model_meta_root, "embeddinginitrange", embeddinginitrange_save);

        cJSON* adam_params_save = cJSON_CreateObject();
        cJSON_AddNumberToObject(adam_params_save, "beta1", adam_params.beta1);
        cJSON_AddNumberToObject(adam_params_save, "beta2", adam_params.beta2);
        cJSON_AddNumberToObject(adam_params_save, "epsilon", adam_params.epsilon);
        cJSON_AddNumberToObject(adam_params_save, "t", adam_params.t);
        cJSON_AddItemToObject(model_meta_root, "adam_params", adam_params_save);

        cJSON_AddNumberToObject(model_meta_root, "step_num", step_num);

        cJSON* transformer_structure_save = cJSON_CreateObject();
        cJSON* layers_save = cJSON_CreateArray();

        for (int index = 0; index < layersAmount; index++){
            cJSON* layer_curr_save = cJSON_CreateObject();
            cJSON* weights_save = cJSON_CreateObject();

            cJSON* normalize_1_save = cJSON_CreateArray();
            char num1[32];
            itoa(index, num1, 10);
            char norm_spath[strlen("layers[].weights.normalize_1") + strlen(num1) + 1];
            sprintf(norm_spath, "layers[%s].weights.normalize_1", num1);
            cJSON_AddItemToArray(normalize_1_save, cJSON_CreateString(norm_spath));
            cJSON_AddItemToObject(weights_save, "normalize_1", normalize_1_save);

            cJSON* normalize_2_save = cJSON_CreateArray();
            sprintf(norm_spath, "layers[%s].weights.normalize_2", num1);
            cJSON_AddItemToArray(normalize_2_save, cJSON_CreateString(norm_spath));
            cJSON_AddItemToObject(weights_save, "normalize_2", normalize_2_save);

            cJSON* attention_save = cJSON_CreateObject();
            cJSON* heads_save = cJSON_CreateArray();
            for (int subindex = 0; subindex < heads; subindex++){
                cJSON* heads_save_curr = cJSON_CreateObject();
                
                cJSON* query_curr = cJSON_CreateArray();
                char num2[32];
                itoa(subindex, num2, 10);
                char query_spath[strlen(num2) + strlen("layers[].weights.attention.heads[].query") + strlen(num1) + 1];
                sprintf(query_spath, "layers[%s].weights.attention.heads[%s].query", num1, num2);
                cJSON_AddItemToArray(query_curr, cJSON_CreateString(query_spath));
                cJSON_AddItemToObject(heads_save_curr, "query", query_curr);

                cJSON* key_curr = cJSON_CreateArray();
                sprintf(query_spath, "layers[%s].weights.attention.heads[%s].key", num1, num2);
                cJSON_AddItemToArray(key_curr, cJSON_CreateString(query_spath));
                cJSON_AddItemToObject(heads_save_curr, "key", key_curr);

                cJSON* value_curr = cJSON_CreateArray();
                sprintf(query_spath, "layers[%s].weights.attention.heads[%s].value", num1, num2);
                cJSON_AddItemToArray(value_curr, cJSON_CreateString(query_spath));
                cJSON_AddItemToObject(heads_save_curr, "value", value_curr);

                cJSON_AddItemToArray(heads_save, heads_save_curr);
            }

            cJSON_AddItemToObject(attention_save, "heads", heads_save);

            cJSON* output_save = cJSON_CreateArray();
            char output_curr_spath[strlen(num1) + strlen("layers[].weights.attention.output") + 1];
            sprintf(output_curr_spath, "layers[%s].weights.attention.output", num1);
            cJSON_AddItemToArray(output_save, cJSON_CreateString(output_curr_spath));
            cJSON_AddItemToObject(attention_save, "output", output_save);

            cJSON_AddItemToObject(weights_save, "attention", attention_save);

            cJSON* ffw_save = cJSON_CreateObject();
            cJSON* ffw_save_grow = cJSON_CreateArray();
            cJSON* ffw_save_gate = cJSON_CreateArray();
            cJSON* ffw_save_shrink = cJSON_CreateArray();
            
            char ffw_spaths[strlen(num1) + strlen("layers[].weights.feed_forward.shrink") + 1];
            sprintf(ffw_spaths, "layers[%s].weights.feed_forward.grow", num1);
            cJSON_AddItemToArray(ffw_save_grow, cJSON_CreateString(ffw_spaths));

            sprintf(ffw_spaths, "layers[%s].weights.feed_forward.gate", num1);
            cJSON_AddItemToArray(ffw_save_gate, cJSON_CreateString(ffw_spaths));

            sprintf(ffw_spaths, "layers[%s].weights.feed_forward.shrink", num1);
            cJSON_AddItemToArray(ffw_save_shrink, cJSON_CreateString(ffw_spaths));

            cJSON_AddItemToObject(ffw_save, "grow", ffw_save_grow);
            cJSON_AddItemToObject(ffw_save, "gate", ffw_save_gate);
            cJSON_AddItemToObject(ffw_save, "shrink", ffw_save_shrink);

            cJSON_AddItemToObject(weights_save, "feed_forward", ffw_save);
            
            cJSON_AddItemToObject(layer_curr_save, "weights", weights_save);


            cJSON* biases_save = cJSON_CreateObject();

            cJSON* normalize_1_biases = cJSON_CreateArray();
            char norm1_bpath[strlen(num1) + strlen("layers[].biases.normalize_1") + 1];
            sprintf(norm1_bpath, "layers[%s].biases.normalize_1", num1);
            cJSON_AddItemToArray(normalize_1_biases, cJSON_CreateString(norm1_bpath));
            cJSON_AddItemToObject(biases_save, "normalize_1", normalize_1_biases);

            cJSON* normalize_2_biases = cJSON_CreateArray();
            sprintf(norm1_bpath, "layers[%s].biases.normalize_2", num1);
            cJSON_AddItemToArray(normalize_2_biases, cJSON_CreateString(norm1_bpath));
            cJSON_AddItemToObject(biases_save, "normalize_2", normalize_2_biases);

            cJSON* attention_biases = cJSON_CreateObject();
            cJSON* heads_biases = cJSON_CreateArray();
            for (int subindex = 0; subindex < heads; subindex++){
                cJSON* heads_biases_curr = cJSON_CreateObject();

                char num2[32];
                itoa(subindex, num2, 10);

                cJSON* query_bias = cJSON_CreateArray();
                char query_bpath[strlen(num2) + strlen("layers[].biases.attention.heads[].query") + strlen(num1) + 1];
                sprintf(query_bpath, "layers[%s].biases.attention.heads[%s].query", num1, num2);
                cJSON_AddItemToArray(query_bias, cJSON_CreateString(query_bpath));
                cJSON_AddItemToObject(heads_biases_curr, "query", query_bias);

                cJSON* key_bias = cJSON_CreateArray();
                sprintf(query_bpath, "layers[%s].biases.attention.heads[%s].key", num1, num2);
                cJSON_AddItemToArray(key_bias, cJSON_CreateString(query_bpath));
                cJSON_AddItemToObject(heads_biases_curr, "key", key_bias);

                cJSON* value_bias = cJSON_CreateArray();
                sprintf(query_bpath, "layers[%s].biases.attention.heads[%s].value", num1, num2);
                cJSON_AddItemToArray(value_bias, cJSON_CreateString(query_bpath));
                cJSON_AddItemToObject(heads_biases_curr, "value", value_bias);

                cJSON_AddItemToArray(heads_biases, heads_biases_curr);
            }
            cJSON_AddItemToObject(attention_biases, "heads", heads_biases);

            cJSON* output_bias = cJSON_CreateArray();
            char output_bpath[strlen(num1) + strlen("layers[].biases.attention.output") + 1];
            sprintf(output_bpath, "layers[%s].biases.attention.output", num1);
            cJSON_AddItemToArray(output_bias, cJSON_CreateString(output_bpath));
            cJSON_AddItemToObject(attention_biases, "output", output_bias);

            cJSON_AddItemToObject(biases_save, "attention", attention_biases);

            cJSON* ffw_biases = cJSON_CreateObject();
            cJSON* ffw_bias_grow = cJSON_CreateArray();
            cJSON* ffw_bias_gate = cJSON_CreateArray();
            cJSON* ffw_bias_shrink = cJSON_CreateArray();

            char ffw_bpaths[strlen(num1) + strlen("layers[].biases.feed_forward.shrink") + 1];
            sprintf(ffw_bpaths, "layers[%s].biases.feed_forward.grow", num1);
            cJSON_AddItemToArray(ffw_bias_grow, cJSON_CreateString(ffw_bpaths));

            sprintf(ffw_bpaths, "layers[%s].biases.feed_forward.gate", num1);
            cJSON_AddItemToArray(ffw_bias_gate, cJSON_CreateString(ffw_bpaths));

            sprintf(ffw_bpaths, "layers[%s].biases.feed_forward.shrink", num1);
            cJSON_AddItemToArray(ffw_bias_shrink, cJSON_CreateString(ffw_bpaths));

            cJSON_AddItemToObject(ffw_biases, "grow", ffw_bias_grow);
            cJSON_AddItemToObject(ffw_biases, "gate", ffw_bias_gate);
            cJSON_AddItemToObject(ffw_biases, "shrink", ffw_bias_shrink);

            cJSON_AddItemToObject(biases_save, "feed_forward", ffw_biases);

            cJSON_AddItemToObject(layer_curr_save, "biases", biases_save);

            cJSON_AddItemToArray(layers_save, layer_curr_save);
        }

        cJSON_AddItemToObject(transformer_structure_save, "layers", layers_save);
        
        cJSON* embeddings_save = cJSON_CreateArray();

        for (int index = 0; index < vocab_len; index++){
            cJSON* embedding_curr_save = cJSON_CreateArray();
            char num__[32];
            itoa(index, num__, 10);
            char spath_curr[strlen(num__) + strlen("embeddings[]") + 1];
            sprintf(spath_curr, "embeddings[%s]", num__);

            cJSON_AddItemToArray(embedding_curr_save, cJSON_CreateString(spath_curr));
            cJSON_AddItemToArray(embeddings_save, embedding_curr_save);
        }

        cJSON_AddItemToObject(transformer_structure_save, "embeddings", embeddings_save);

        cJSON* vocab_projection_save = cJSON_CreateObject();
        cJSON* vocab_projection_save_weights = cJSON_CreateArray();
        cJSON* vocab_projection_save_biases = cJSON_CreateArray();

        for (int index = 0; index < vocab_len; index++){
            cJSON* vocab_projection_raw_weights_curr = cJSON_CreateArray();
            cJSON* vocab_projection_raw_biases_curr = cJSON_CreateArray();
            char num__[32];
            itoa(index, num__, 10);
            char spath_curr[strlen(num__) + strlen("vocab_projection.weights[]") + 1];
            sprintf(spath_curr, "vocab_projection.weights[%s]", num__);
            
            cJSON_AddItemToArray(vocab_projection_raw_weights_curr, cJSON_CreateString(spath_curr));

            sprintf(spath_curr, "vocab_projection.biases[%s]", num__);

            cJSON_AddItemToArray(vocab_projection_raw_biases_curr, cJSON_CreateString(spath_curr));

            cJSON_AddItemToArray(vocab_projection_save_weights, vocab_projection_raw_weights_curr);
            cJSON_AddItemToArray(vocab_projection_save_biases, vocab_projection_raw_biases_curr);
        }

        cJSON_AddItemToObject(vocab_projection_save, "weights", vocab_projection_save_weights);
        cJSON_AddItemToObject(vocab_projection_save, "biases", vocab_projection_save_biases);

        cJSON_AddItemToObject(transformer_structure_save, "vocab_projection", vocab_projection_save);

        cJSON_AddItemToObject(model_meta_root, "transformer_structure", transformer_structure_save);

        char* model_meta_save = cJSON_PrintUnformatted(model_meta_root);
        size_t model_meta_save_len = strlen(model_meta_save);

        dprintf("Size of model_meta.json: %zu bytes.\n", model_meta_save_len);

        cJSON_Delete(model_meta_root);

        //now zip it
        mz_zip_archive zipfile;
        memset(&zipfile, 0, sizeof(zipfile));
        
        if (!mz_zip_writer_init_file(&zipfile, filepath, 0)){
            printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
            return false;
        }

        if (!mz_zip_writer_add_mem(&zipfile, "model_meta.json", model_meta_save, model_meta_save_len, MZ_NO_COMPRESSION)){
            printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
            mz_zip_writer_end(&zipfile);
            return false;
        }

        bool write_param_to_zip(const char* name, param p, size_t count){
            size_t sz = count * 3 * sizeof(float);
            float* buf = malloc(sz);
            if (!buf){
                printf("Failed to allocate memory to save model.\n");
                return false;
            }
            memcpy(buf, p.param, count * sizeof(float));
            if (p.m){
                memcpy(buf + count, p.m, count * sizeof(float));
            } else {
                memset(buf + count, 0, count * sizeof(float));
            }
            if (p.v){
                memcpy(buf + count * 2, p.v, count * sizeof(float));
            } else {
                memset(buf + count * 2, 0, count * sizeof(float));
            }
            bool ok = mz_zip_writer_add_mem(&zipfile, name, buf, sz, MZ_NO_COMPRESSION);
            free(buf);
            return ok;
        }

        for (int index = 0; index < layersAmount; index++){
            char _num[32];
            itoa(index, _num, 10);
            char normalize_path[strlen(_num) + strlen("layers[].weights.normalize_1") + 1];
            sprintf(normalize_path, "layers[%s].weights.normalize_1", _num);

            if (!write_param_to_zip(normalize_path, layers[index].weights.normalize_1, embeddingSize)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            sprintf(normalize_path, "layers[%s].weights.normalize_2", _num);

            if (!write_param_to_zip(normalize_path, layers[index].weights.normalize_2, embeddingSize)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            for (int subindex = 0; subindex < heads; subindex++){
                char _num2[32];
                itoa(subindex, _num2, 10);
                char head_data_path[strlen(_num) + strlen(_num2) + strlen("layers[].weights.attention.heads[].query") + 1];
                sprintf(head_data_path, "layers[%s].weights.attention.heads[%s].query", _num, _num2);

                if (!write_param_to_zip(head_data_path, layers[index].weights.attention.heads[subindex].query, head_dim * embeddingSize)){
                    printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                    mz_zip_writer_end(&zipfile);
                    return false;
                }

                sprintf(head_data_path, "layers[%s].weights.attention.heads[%s].key", _num, _num2);
                if (!write_param_to_zip(head_data_path, layers[index].weights.attention.heads[subindex].key, head_dim * embeddingSize)){
                    printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                    mz_zip_writer_end(&zipfile);
                    return false;
                }

                sprintf(head_data_path, "layers[%s].weights.attention.heads[%s].value", _num, _num2);
                if (!write_param_to_zip(head_data_path, layers[index].weights.attention.heads[subindex].value, head_dim * embeddingSize)){
                    printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                    mz_zip_writer_end(&zipfile);
                    return false;
                }
            }

            char attn_o_path[strlen(_num) + strlen("layers[].weights.attention.output") + 1];
            sprintf(attn_o_path, "layers[%s].weights.attention.output", _num);
            if (!write_param_to_zip(attn_o_path, layers[index].weights.attention.output, embeddingSize * (head_dim * heads))){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            char ffw_paths[strlen(_num) + strlen("layers[].weights.feed_forward.shrink") + 1];
            sprintf(ffw_paths, "layers[%s].weights.feed_forward.grow", _num);
            if (!write_param_to_zip(ffw_paths, layers[index].weights.feed_forward.grow, embeddingSize * (embeddingSize * ffnGrowSize))){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            sprintf(ffw_paths, "layers[%s].weights.feed_forward.gate", _num);
            if (!write_param_to_zip(ffw_paths, layers[index].weights.feed_forward.gate, embeddingSize * (embeddingSize * ffnGrowSize))){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            sprintf(ffw_paths, "layers[%s].weights.feed_forward.shrink", _num);
            if (!write_param_to_zip(ffw_paths, layers[index].weights.feed_forward.shrink, embeddingSize * (embeddingSize * ffnGrowSize))){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            //also do biases

            itoa(index, _num, 10);
            char norm_b_path[strlen(_num) + strlen("layers[].biases.normalize_1") + 1];
            sprintf(norm_b_path, "layers[%s].biases.normalize_1", _num);
            if (!write_param_to_zip(norm_b_path, layers[index].biases.normalize_1, embeddingSize)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }
            sprintf(norm_b_path, "layers[%s].biases.normalize_2", _num);
            if (!write_param_to_zip(norm_b_path, layers[index].biases.normalize_2, embeddingSize)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }
            for (int subindex = 0; subindex < heads; subindex++){
                char _num2[32];
                itoa(subindex, _num2, 10);
                char head_data_path[strlen(_num) + strlen(_num2) + strlen("layers[].biases.attention.heads[].query") + 1];
                sprintf(head_data_path, "layers[%s].biases.attention.heads[%s].query", _num, _num2);

                if (!write_param_to_zip(head_data_path, layers[index].biases.attention.heads[subindex].query, head_dim)){
                    printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                    mz_zip_writer_end(&zipfile);
                    return false;
                }

                sprintf(head_data_path, "layers[%s].biases.attention.heads[%s].key", _num, _num2);
                if (!write_param_to_zip(head_data_path, layers[index].biases.attention.heads[subindex].key, head_dim)){
                    printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                    mz_zip_writer_end(&zipfile);
                    return false;
                }

                sprintf(head_data_path, "layers[%s].biases.attention.heads[%s].value", _num, _num2);
                if (!write_param_to_zip(head_data_path, layers[index].biases.attention.heads[subindex].value, head_dim)){
                    printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                    mz_zip_writer_end(&zipfile);
                    return false;
                }
            }

            char attn_o_path_[strlen(_num) + strlen("layers[].biases.attention.output") + 1];
            sprintf(attn_o_path_, "layers[%s].biases.attention.output", _num);
            if (!write_param_to_zip(attn_o_path_, layers[index].biases.attention.output, embeddingSize)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            char ffw_paths_[strlen(_num) + strlen("layers[].biases.feed_forward.shrink") + 1];
            sprintf(ffw_paths_, "layers[%s].biases.feed_forward.grow", _num);
            if (!write_param_to_zip(ffw_paths_, layers[index].biases.feed_forward.grow, (embeddingSize * ffnGrowSize))){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            sprintf(ffw_paths_, "layers[%s].biases.feed_forward.gate", _num);
            if (!write_param_to_zip(ffw_paths_, layers[index].biases.feed_forward.gate, (embeddingSize * ffnGrowSize))){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            sprintf(ffw_paths_, "layers[%s].biases.feed_forward.shrink", _num);
            if (!write_param_to_zip(ffw_paths_, layers[index].biases.feed_forward.shrink, embeddingSize)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }
        }

        for (int index = 0; index < vocab_len; index++){
            char _num[32];
            itoa(index, _num, 10);
            char embeddingPath[strlen(_num) + strlen("embeddings[]") + 1];
            sprintf(embeddingPath, "embeddings[%s]", _num);

            if (!write_param_to_zip(embeddingPath, embeddings[index], embeddingSize)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }
        }

        for (int index = 0; index < vocab_len; index++){
            char _num[32];
            itoa(index, _num, 10);
            char vocab_projection_path[strlen(_num) + strlen("vocab_projection.weights[]") + 1];
            sprintf(vocab_projection_path, "vocab_projection.weights[%s]", _num);

            if (!write_param_to_zip(vocab_projection_path, (param){
                .param = &vocab_projection.weights.param[index * embeddingSize],
                .m = &vocab_projection.weights.m[index * embeddingSize],
                .v = &vocab_projection.weights.v[index * embeddingSize],
            }, embeddingSize)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }

            sprintf(vocab_projection_path, "vocab_projection.biases[%s]", _num);
            if (!write_param_to_zip(vocab_projection_path, (param){
                .param = &vocab_projection.biases.param[index],
                .m = &vocab_projection.biases.m[index],
                .v = &vocab_projection.biases.v[index],
            }, 1)){
                printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
                mz_zip_writer_end(&zipfile);
                return false;
            }
        }

        if (!mz_zip_writer_finalize_archive(&zipfile)) {
            printf("Failed to save model at path \"%s\". Common causes are: Not enough storage space or no permissions.\n", filepath);
            mz_zip_writer_end(&zipfile);
            return false;
        }

        mz_zip_writer_end(&zipfile);
        free(model_meta_save);

        printf("Saved model at path \"%s\" in %lldms.\n", filepath, timer_end(save_timer));

        return true;
    }

    void calculate_rope_tables(int sequence_length, float*** cos_table, float*** sin_table){
        float** cos_local = malloc(sequence_length * sizeof(float*));
        float** sin_local = malloc(sequence_length * sizeof(float*));
        if ((!cos_local) || (!sin_local)){
            printf("Failed to allocate memory to compute rotary embeddings.\n");
            exit(1);
        }
        for (int index = 0; index < sequence_length; index++){
            cos_local[index] = malloc(head_dim * sizeof(float));
            sin_local[index] = malloc(head_dim * sizeof(float));
            if ((!cos_local[index]) || (!sin_local[index])){
                printf("Failed to allocate memory to compute rotary embeddings.\n");
                exit(1);
            }
            for (int subindex = 0; subindex < head_dim; subindex += 2){
                float denominator = powf(10000.0f, (float)(subindex) / (float)(head_dim));
                float theta = index / denominator;
                float c = cosf(theta);
                float s = sinf(theta);
                cos_local[index][subindex] = c;
                sin_local[index][subindex] = s;
                if (subindex + 1 < head_dim){
                    cos_local[index][subindex + 1] = c;
                    sin_local[index][subindex + 1] = s;
                }
            }
        }
        *cos_table = cos_local;
        *sin_table = sin_local;
    }

    param* get_embedding(int id){
        if (!id_to_token(id)){
            return NULL; //Invalid token.
        }
        return &embeddings[id];
    }

    float calculate_inv_rms(float* in, int in_len){
        float mean_sq = 0;
        for (int index = 0; index < in_len; index++){
            mean_sq += in[index] * in[index];
        }
        mean_sq = mean_sq / in_len;
        float epsilon = 1e-5f;
        float inv_rms = 1.0f / sqrtf(mean_sq + epsilon);
        return inv_rms;
    }

    float* _calculate_x_hat_only(float* in, int in_len){
        if (!in){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return NULL;
        }
        if (in_len < 1){
            return NULL;
        }
        float mean_sq = 0;
        for (int index = 0; index < in_len; index++){
            mean_sq += in[index] * in[index];
        }
        mean_sq = mean_sq / in_len;

        float epsilon = 1e-5f;
        float inv_rms = 1.0f / sqrtf(mean_sq + epsilon);
        
        float* x_hat = malloc(in_len * sizeof(float));
        if (!x_hat){
            printf("Failed memory allocation to calculate x hat.\n");
            exit(1);
        }
        for (int index = 0; index < in_len; index++){
            x_hat[index] = in[index] * inv_rms;
        }

        return x_hat;
    }

    float* normalize_vector(float* vec, int vec_len, param* g, param* b){
        if (!vec){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return NULL;
        }
        if (vec_len < 1){
            return NULL;
        }
        if (!g || !g->param){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return NULL;
        }
        float* x_hat = _calculate_x_hat_only(vec, vec_len);
        if (!x_hat){
            printf("Failed to calculate x_hat.\n"); //In case physics have broken down or a bitflip or idk
            exit(1);
        }
        for (int index = 0; index < vec_len; index++){
            float bias_val = (b && b->param ? b->param[index] : 0.0f);
            x_hat[index] = x_hat[index] * g->param[index] + bias_val;
        }

        return x_hat;
    }

    float dot_product(float* vec1, int vec1_len, float* vec2, int vec2_len){
        float sum = 0;
        if (vec1_len != vec2_len){
            return -1;
        }
        if (vec1_len < 1){
            return -1;
        }
        if (!vec1){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return -1;
        }
        if (!vec2){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return -1;
        }
        for (int index = 0; index < vec1_len; index++){
            sum += vec1[index] * vec2[index];
        }
        return sum;
    }

    float* add_vectors(float* vec1, int vec1_len, float* vec2, int vec2_len){
        if (vec1_len != vec2_len){
            return NULL;
        }
        if (vec1_len < 1){
            return NULL;
        }
        if (!vec1){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return NULL;
        }
        if (!vec2){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return NULL;
        }

        float* new_vec = malloc(vec1_len * sizeof(float));
        if (!new_vec){
            printf("Failed memory allocation to add vectors.\n");
            exit(1);
        }
        
        for (int index = 0; index < vec1_len; index++){
            new_vec[index] = vec1[index] + vec2[index];
        }

        return new_vec;
    }

    float sigmoid_scalar(float x){
        return 1.0f / (1.0f + expf(-x));
    }

    float* softmax(float* vec, int vec_len, float* new_vec_ptr){
        if (!vec){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return NULL;
        }
        if (!new_vec_ptr){
            printf("Null dereference caught from: %p.\n", __builtin_return_address(0));
            return NULL;
        }
        if (vec_len < 1){
            return NULL;
        }
        
        float max = -__FLT_MAX__;
        for (int index = 0; index < vec_len; index++){
            if (vec[index] > max){
                max = vec[index];
            }
        }
        
        float* rets = new_vec_ptr;
        
        for (int index = 0; index < vec_len; index++){
            rets[index] = expf(vec[index] - max);
        }

        long double exp_sum = 0;
        for (int index = 0; index < vec_len; index++){
            exp_sum += rets[index];
        }
        if (exp_sum == 0){
            for (int index = 0; index < vec_len; index++){
                rets[index] = 1.0f / (float)(vec_len);
            }
            return rets;
        }
        for (int index = 0; index < vec_len; index++){
            rets[index] = rets[index] / (float)(exp_sum);
        }
        return rets;
    }

    typedef struct {
        float** norm1_x_hat;
        float* norm1_inv_rms;
        struct {
            float** q_vectors;
            float** k_vectors;
            float** v_vectors;
            float** attention_scores;
            float** attention_probs;
            float** output;
        }* heads;
        float** combined;
        float** attn_dropout_mask;
        float** norm2_x_hat;
        float* norm2_inv_rms;
        float** norm2_output;
        float** normalized;
        struct {
            float** bigger;
            float** after_relu;
            float** final;
            float** ff_dropout_mask;
        } feed_forward;
    } layer_cache_entry;

    typedef struct {
        int predicted_token_id;
        struct {
            int* tokenized;
            size_t seq_len;
            float** initial_embeddings;
            float** positional_encodings;
            float** vocab_scores;
            float** final_embeddings;
            layer_cache_entry* layers;
        } cache;
        bool success;
    } infret;

    void free_cache_object(typeof(((infret*)0)->cache) cache_object){
        int seq_len = cache_object.seq_len;
        if (cache_object.tokenized){
            free(cache_object.tokenized);
        }
        if (cache_object.initial_embeddings){
            for (int index = 0; index < seq_len; index++){
                free(cache_object.initial_embeddings[index]);
            }
            free(cache_object.initial_embeddings);
        }
        if (cache_object.positional_encodings){
            for (int index = 0; index < seq_len; index++){
                free(cache_object.positional_encodings[index]);
            }
            free(cache_object.positional_encodings);
        }
        if (cache_object.vocab_scores){
            for (int index = 0; index < seq_len; index++){
                free(cache_object.vocab_scores[index]);
            }
            free(cache_object.vocab_scores);
        }
        if (cache_object.final_embeddings){
            bool skip_free = false;
            if (layersAmount > 0){
                if (cache_object.final_embeddings == cache_object.layers[layersAmount - 1].feed_forward.final){
                    skip_free = true;
                }
            }
            if (!skip_free){
                for (int index = 0; index < seq_len; index++){
                    free(cache_object.final_embeddings[index]);
                }
                free(cache_object.final_embeddings);
            }
        }

        for (int index = 0; index < layersAmount; index++){
            for (int subindex = 0; subindex < seq_len; subindex++){
                free(cache_object.layers[index].norm1_x_hat[subindex]);
                free(cache_object.layers[index].norm2_x_hat[subindex]);
                free(cache_object.layers[index].norm2_output[subindex]);
                free(cache_object.layers[index].normalized[subindex]);
                free(cache_object.layers[index].combined[subindex]);
                free(cache_object.layers[index].feed_forward.bigger[subindex]);
                free(cache_object.layers[index].feed_forward.after_relu[subindex]);
                free(cache_object.layers[index].feed_forward.final[subindex]);
                if (cache_object.layers[index].attn_dropout_mask){
                    free(cache_object.layers[index].attn_dropout_mask[subindex]);
                }
                if (cache_object.layers[index].feed_forward.ff_dropout_mask){
                    free(cache_object.layers[index].feed_forward.ff_dropout_mask[subindex]);
                }
            }
            free(cache_object.layers[index].norm1_x_hat);
            free(cache_object.layers[index].norm1_inv_rms);
            free(cache_object.layers[index].norm2_x_hat);
            free(cache_object.layers[index].norm2_inv_rms);
            free(cache_object.layers[index].norm2_output);
            free(cache_object.layers[index].normalized);
            free(cache_object.layers[index].combined);
            free(cache_object.layers[index].feed_forward.bigger);
            free(cache_object.layers[index].feed_forward.after_relu);
            free(cache_object.layers[index].feed_forward.final);
            if (cache_object.layers[index].attn_dropout_mask){
                free(cache_object.layers[index].attn_dropout_mask);
            }
            if (cache_object.layers[index].feed_forward.ff_dropout_mask){
                free(cache_object.layers[index].feed_forward.ff_dropout_mask);
            }

            for (int subindex = 0; subindex < heads; subindex++){
                for (int subindex_ = 0; subindex_ < seq_len; subindex_++){
                    free(cache_object.layers[index].heads[subindex].q_vectors[subindex_]);
                    free(cache_object.layers[index].heads[subindex].k_vectors[subindex_]);
                    free(cache_object.layers[index].heads[subindex].v_vectors[subindex_]);
                    free(cache_object.layers[index].heads[subindex].attention_scores[subindex_]);
                    free(cache_object.layers[index].heads[subindex].attention_probs[subindex_]);
                    free(cache_object.layers[index].heads[subindex].output[subindex_]);
                }
                free(cache_object.layers[index].heads[subindex].q_vectors);
                free(cache_object.layers[index].heads[subindex].k_vectors);
                free(cache_object.layers[index].heads[subindex].v_vectors);
                free(cache_object.layers[index].heads[subindex].attention_scores);
                free(cache_object.layers[index].heads[subindex].attention_probs);
                free(cache_object.layers[index].heads[subindex].output);
            }

            free(cache_object.layers[index].heads);
        }
        free(cache_object.layers);
    }

    infret inference(int* tokens, size_t seq_len, bool return_cache, float temperature, bool verbose){
        void verbprintf(const char *format, ...) {
            if (!verbose) {
                return;
            }

            va_list args;
            va_start(args, format);
            vprintf(format, args);
            va_end(args);
        }

        //Memory tracking system
        void** tracked = NULL;
        size_t tracked_len = 0;
        bool track_enabled = !return_cache;

        int cmp_ptr(const void* a, const void* b){
            void* pa = *(void**)a;
            void* pb = *(void**)b;
            return (pa > pb) - (pa < pb); // or: return (uintptr_t)pa - (uintptr_t)pb;
        }

        void sort_tracked(){
            qsort(tracked, tracked_len, sizeof(void*), cmp_ptr);
        }

        ssize_t binary_search_tracked(void* key){
            size_t left = 0;
            size_t right = tracked_len;

            while (left < right) {
                size_t mid = left + (right - left) / 2;
                void* mid_ptr = tracked[mid];

                if (key == mid_ptr) {
                    return (ssize_t)mid;
                } else if (key < mid_ptr) {
                    right = mid;
                } else {
                    left = mid + 1;
                }
            }
            return -1;
        }

        //Failed alloc procedure.
        void failed_alloc(char* failed_task){
            printf("%s.\n", failed_task);
            exit(1); //let os reclaim memory lol
        }

        void* track(void* ptr, char* failed_task){
            if (!ptr){
                //Failed alloc = no memory = can't do anything anyways = should exit.
                failed_alloc(failed_task);
            }
            if (!track_enabled){
                return ptr;
            }
            tracked_len++;
            void** tmp = realloc(tracked, tracked_len * sizeof(void*));
            if (!tmp){
                printf("Failed to allocate memory to update freelist.\n");
                tracked_len--;
                free(ptr);
                return NULL;
            }
            tracked = tmp;
            tracked[tracked_len - 1] = ptr;
            sort_tracked();
            return ptr;
        }

        void cleanup(){
            for (int index = 0; index < tracked_len; index++){
                free(tracked[index]);
            }
            free(tracked);
            tracked = NULL;
            tracked_len = 0;
            return;
        }

        bool untrack(void* ptr){
            ssize_t index_ptr = binary_search_tracked(ptr);
            if (index_ptr == -1){
                return false;
            }

            tracked_len--;
            void** new = malloc(tracked_len * sizeof(void*));
            if (!new){
                tracked_len++;
                return false;
            }

            bool offset = false;
            for (ssize_t index = 0; index < tracked_len + 1; index++){
                if (index == index_ptr){
                    offset = true;
                    continue;
                }
                if (!offset){
                    new[index] = tracked[index];
                }
                else{
                    new[index - 1] = tracked[index];
                }
            }

            free(tracked);
            tracked = new;
            return true;
        }

        verbprintf("Doing inference...\n");
        long long timer_ = timer();

        infret rets = {0}; //zero every field

        rets.success = true; //failure will set it to false, its a sucess otherwise.
        if (head_dim < 1){
            printf("Head dimension is not initalized.\n");
            rets.success = false;
            cleanup();
            return rets;
        }

        // Copy tokens to cache if needed, otherwise use directly
        int* tokenized = NULL;
        if (return_cache){
            tokenized = malloc(seq_len * sizeof(int));
            if (!tokenized){
                printf("Failed to allocate memory to copy tokens for cache.\n");
                rets.success = false;
                cleanup();
                return rets;
            }
            memcpy(tokenized, tokens, seq_len * sizeof(int));
        } else {
            tokenized = (int*)tokens; // Just use the pointer directly
        }

        if (seq_len > contextSize){
            printf("Provided context is larger than context size, cannot proceed.\n");
            rets.success = false;
            cleanup();
            return rets;
        }

        long long subtimer = timer();
        verbprintf("Computing rotary tables...\n");
        float** rope_cos = NULL;
        float** rope_sin = NULL;
        rets.cache.tokenized = return_cache ? tokenized : NULL;
        rets.cache.seq_len = seq_len;
        rets.cache.initial_embeddings = NULL;
        rets.cache.positional_encodings = NULL;
        calculate_rope_tables(seq_len, &rope_cos, &rope_sin);
        for (int index = 0; index < seq_len; index++){
            track(rope_cos[index], "Failed to allocate memory to compute rotary embeddings.");
            track(rope_sin[index], "Failed to allocate memory to compute rotary embeddings.");
        }
        track(rope_cos, "Failed to allocate memory to compute rotary embeddings.");
        track(rope_sin, "Failed to allocate memory to compute rotary embeddings.");
        verbprintf("Computed rotary tables in %lldms.\n", timer_end(subtimer));

        subtimer = timer();
        verbprintf("Computing final embeddings...\n");
        
        float** final_embeddings = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute final embeddings.");

        for (int index = 0; index < seq_len; index++){
            int token_id = tokenized[index];
            param* embedding = get_embedding(token_id);
            float* final_embedding = NULL;
            final_embedding = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to process embeddings.");
            for (int subindex = 0; subindex < embeddingSize; subindex++){
                final_embedding[subindex] = embedding->param[subindex];
            }

            final_embeddings[index] = final_embedding;
        }

        verbprintf("Computed final embeddings in %lldms.\n", timer_end(subtimer));

        long long layerstimer = timer();
        verbprintf("Computing layers...\n");
        if (return_cache){
            rets.cache.layers = calloc(layersAmount * sizeof(layer_cache_entry), 1);
            if (!rets.cache.layers){
                failed_alloc("Failed to allocate memory to compute layers.");
            }
        }
        for (int layer = 0; layer < layersAmount; layer++){
            long long layertimer = timer();
            verbprintf("Computing layer %d/%d...\n", layer, layersAmount);
            
            float** normalized_embeddings = NULL;
            if (!return_cache){
                normalized_embeddings = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute normalized embeddings.");
            }
            else{
                normalized_embeddings = malloc(seq_len * sizeof(float*));
                if (!normalized_embeddings){
                    failed_alloc("Failed to allocate memory to compute normalized embeddings.");
                }
            }

            param* norm1_weights = &layers[layer].weights.normalize_1;
            param* norm1_biases = &layers[layer].biases.normalize_1;
            if (return_cache){
                rets.cache.layers[layer].norm1_x_hat = malloc(seq_len * sizeof(float*));
                if (!rets.cache.layers[layer].norm1_x_hat){
                    failed_alloc("Failed to allocate memory to cache normalize 1 x hats.");
                }
                rets.cache.layers[layer].norm1_inv_rms = malloc(seq_len * sizeof(float));
                if (!rets.cache.layers[layer].norm1_inv_rms){
                    failed_alloc("Failed to allocate memory to cache norm1 inv rms.");
                }
            }

            for (int index = 0; index < seq_len; index++){
                float* x_hat_for_cache_1;
                float inv_rms_1 = 0;
                if (return_cache){
                    x_hat_for_cache_1 = _calculate_x_hat_only(final_embeddings[index], embeddingSize);
                
                    if (!x_hat_for_cache_1){
                        failed_alloc("Failed to compute x hat for cache 1.");
                    }
                    rets.cache.layers[layer].norm1_x_hat[index] = x_hat_for_cache_1;
                    inv_rms_1 = calculate_inv_rms(final_embeddings[index], embeddingSize);
                    rets.cache.layers[layer].norm1_inv_rms[index] = inv_rms_1;
                }
                else{
                    inv_rms_1 = calculate_inv_rms(final_embeddings[index], embeddingSize);
                }

                float* final_norm_1_output = NULL;
                if (!return_cache){
                    final_norm_1_output = track(normalize_vector(final_embeddings[index], embeddingSize, norm1_weights, norm1_biases), "Failed to compute x hat for cache 1.");
                }
                else{
                    final_norm_1_output = normalize_vector(final_embeddings[index], embeddingSize, norm1_weights, norm1_biases);
                    if (!final_norm_1_output){
                        failed_alloc("Failed to compute x hat for cache 1.");
                    }
                }

                normalized_embeddings[index] = final_norm_1_output;
            }

            if (return_cache){
                rets.cache.layers[layer].normalized = normalized_embeddings;
            }

            float*** head_outputs = track(malloc(heads * sizeof(float**)), "Failed to allocate memory to store head outputs."); //too many pointers i swear
            
            if (return_cache) {
                if (!rets.cache.layers[layer].heads) {
                    rets.cache.layers[layer].heads = calloc(heads * sizeof(*rets.cache.layers[layer].heads), 1);
                    if (!rets.cache.layers[layer].heads){
                        failed_alloc("Failed to allocate memory to store head cache.");
                    }
                }
            }

            for (int head = 0; head < heads; head++){
                float** q_vectors = NULL;
                float** k_vectors = NULL;
                float** v_vectors = NULL;
                if (return_cache){
                    q_vectors = malloc(seq_len * sizeof(float*));
                    k_vectors = malloc(seq_len * sizeof(float*));
                    v_vectors = malloc(seq_len * sizeof(float*));

                    if ((!q_vectors) || (!k_vectors) || (!v_vectors)){
                        failed_alloc("Failed to allocate memory to store q/k/v vectors.");
                    }
                }
                else{
                    q_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to store q vectors.");
                    k_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to store k vectors.");
                    v_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to store v vectors.");
                }

                for (int index = 0; index < seq_len; index++){
                    if (return_cache){
                    q_vectors[index] = calloc(head_dim * sizeof(float), 1);
                    k_vectors[index] = calloc(head_dim * sizeof(float), 1);
                    v_vectors[index] = calloc(head_dim * sizeof(float), 1);

                        if ((!q_vectors[index]) || (!k_vectors[index]) || (!v_vectors[index])){
                            failed_alloc("Failed to allocate memory to store q/k/v vectors.");
                        }
                    }
                    else{
                        q_vectors[index] = track(calloc(head_dim * sizeof(float), 1), "Failed to allocate memory to store q vectors.");
                        k_vectors[index] = track(calloc(head_dim * sizeof(float), 1), "Failed to allocate memory to store k vectors.");
                        v_vectors[index] = track(calloc(head_dim * sizeof(float), 1), "Failed to allocate memory to store v vectors.");
                    }
                }

                float** attention_scores = NULL;
                if (return_cache){
                    attention_scores = malloc(seq_len * sizeof(float*));
                    if (!attention_scores){
                        failed_alloc("Failed to allocate memory to store attention scores.");
                    }
                }
                else{
                    attention_scores = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to store attention scores.");
                }

                for (int index = 0; index < seq_len; index++){
                    if (return_cache){
                        attention_scores[index] = calloc(seq_len * sizeof(float), 1);
                        if (!attention_scores[index]){
                            failed_alloc("Failed to allocate memory to store attention scores.");
                        }
                    }
                    else{
                        attention_scores[index] = track(calloc(seq_len * sizeof(float), 1), "Failed to allocate memory to store attention scores.");
                    }
                }

                float** attention_probs = NULL;
                if (return_cache){
                    attention_probs = malloc(seq_len * sizeof(float*));
                    if (!attention_probs){
                        failed_alloc("Failed to allocate memory to store attention probs.");
                    }
                }
                else{
                    attention_probs = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to store attention probs.");
                }

                for (int index = 0; index < seq_len; index++){
                    if (return_cache){
                        attention_probs[index] = calloc(seq_len * sizeof(float), 1);
                        if (!attention_probs[index]){
                            failed_alloc("Failed to allocate memory to store attention probs.");
                        }
                    }
                    else{
                        attention_probs[index] = track(calloc(seq_len * sizeof(float), 1), "Failed to allocate memory to store attention probs.");
                    }
                }

                float** post_attention_vectors = NULL;
                if (return_cache){
                    post_attention_vectors = malloc(seq_len * sizeof(float*));
                    if (!post_attention_vectors){
                        failed_alloc("Failed to allocate memory to store post attention vectors.");
                    }
                }
                else{
                    post_attention_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to store post attention vectors.");
                }

                for (int index = 0; index < seq_len; index++){
                    if (return_cache){
                        post_attention_vectors[index] = calloc(head_dim * sizeof(float), 1);
                        if (!post_attention_vectors[index]){
                            failed_alloc("Failed to allocate memory to store post attention vectors.");
                        }
                    }
                    else{
                        post_attention_vectors[index] = track(calloc(head_dim * sizeof(float), 1), "Failed to allocate memory to store post attention vectors.");
                    }
                }

                typeof(layers[layer].weights.attention.heads[head]) head_weights = layers[layer].weights.attention.heads[head];
                typeof(layers[layer].biases.attention.heads[head]) head_biases = layers[layer].biases.attention.heads[head];

                param* head_query_weights = &head_weights.query;
                param* head_key_weights = &head_weights.key;
                param* head_value_weights = &head_weights.value;

                param* head_query_biases = &head_biases.query;
                param* head_key_biases = &head_biases.key;
                param* head_value_biases = &head_biases.value;

                for (int index = 0; index < seq_len; index++){
            float* token_embedding = normalized_embeddings[index];
            // Q
            for (int pos = 0; pos < head_dim; pos++){
                float q_sum = 0;
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    q_sum += token_embedding[subindex] * head_query_weights->param[pos * embeddingSize + subindex];
                }
                        q_vectors[index][pos] = q_sum + head_query_biases->param[pos];
                    }
                    // K
                    for (int pos = 0; pos < head_dim; pos++){
                        float k_sum = 0;
                        for (int subindex = 0; subindex < embeddingSize; subindex++){
                            k_sum += token_embedding[subindex] * head_key_weights->param[pos * embeddingSize + subindex];
                        }
                        k_vectors[index][pos] = k_sum + head_key_biases->param[pos];
                    }

                    // V
                    for (int pos = 0; pos < head_dim; pos++){
                        float v_sum = 0;
                        for (int subindex = 0; subindex < embeddingSize; subindex++){
                            v_sum += token_embedding[subindex] * head_value_weights->param[pos * embeddingSize + subindex];
                        }
                        v_vectors[index][pos] = v_sum + head_value_biases->param[pos];
                    }
                    for (int subindex = 0; subindex + 1 < head_dim; subindex += 2){
                        float c = rope_cos[index][subindex];
                        float s = rope_sin[index][subindex];
                        float q_even = q_vectors[index][subindex];
                        float q_odd = q_vectors[index][subindex + 1];
                        float k_even = k_vectors[index][subindex];
                        float k_odd = k_vectors[index][subindex + 1];
                        q_vectors[index][subindex] = q_even * c - q_odd * s;
                        q_vectors[index][subindex + 1] = q_even * s + q_odd * c;
                        k_vectors[index][subindex] = k_even * c - k_odd * s;
                        k_vectors[index][subindex + 1] = k_even * s + k_odd * c;
                    }
                }
                
                //Attention scores
                for (int index = 0; index < seq_len; index++){
                    for (int subindex = 0; subindex < seq_len; subindex++){
                        if (subindex > index){
                            attention_scores[index][subindex] = -1e9f;
                            continue;
                        }
                        attention_scores[index][subindex] = dot_product(q_vectors[index], head_dim, k_vectors[subindex], head_dim);
                    }
                }

                for (int index = 0; index < seq_len; index++){
                    for (int subindex = 0; subindex < seq_len; subindex++){
                        if (attention_scores[index][subindex] != -1e9f){
                            attention_scores[index][subindex] /= sqrtf((float)(head_dim));
                        }
                    }
                }

                for (int index = 0; index < seq_len; index++){
                    attention_probs[index] = softmax(attention_scores[index], seq_len, attention_probs[index]);
                }

                for (int index = 0; index < seq_len; index++){
                    for (int subindex = 0; subindex < head_dim; subindex++){
                        for (int subindex_ = 0; subindex_ < seq_len; subindex_++){
                            post_attention_vectors[index][subindex] += v_vectors[subindex_][subindex] * attention_probs[index][subindex_];
                        }
                    }
                }

                head_outputs[head] = post_attention_vectors;

                if (return_cache){
                    rets.cache.layers[layer].heads[head].q_vectors = q_vectors;
                    rets.cache.layers[layer].heads[head].k_vectors = k_vectors;
                    rets.cache.layers[layer].heads[head].v_vectors = v_vectors;
                    rets.cache.layers[layer].heads[head].attention_scores = attention_scores;
                    rets.cache.layers[layer].heads[head].attention_probs = attention_probs;
                    rets.cache.layers[layer].heads[head].output = post_attention_vectors;
                }
            }

            float** combined_vectors = NULL;
            if (return_cache){
                combined_vectors = malloc(seq_len * sizeof(float*));
                if (!combined_vectors){
                    failed_alloc("Failed to allocate memory to store combined vectors.");
                }
            }
            else{
                combined_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to store combined vectors.");
            }

            for (int index = 0; index < seq_len; index++){
                if (return_cache){
                    combined_vectors[index] = calloc(embeddingSize * sizeof(float), 1);
                    if (!combined_vectors[index]){
                        failed_alloc("Failed to allocate memory to store combined vectors.");
                    }
                }
                else{
                    combined_vectors[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to store combined vectors.");
                }
            }

            float* concatenated = calloc(head_dim * heads * sizeof(float), 1);
            if (!concatenated){
                failed_alloc("Failed to allocate memory to store concatenated heads.");
            }
            float* output_vector = calloc(embeddingSize * sizeof(float), 1);
            if (!output_vector){
                failed_alloc("Failed to allocate memory to store output vector.");
            }
            
            param* output_weights = &layers[layer].weights.attention.output;
            param* output_biases = &layers[layer].biases.attention.output;

            for (int index = 0; index < seq_len; index++){
                int current_offset = 0;
                for (int subindex = 0; subindex < heads; subindex++){
                    memcpy(concatenated + current_offset, head_outputs[subindex][index], head_dim * sizeof(float));
                    current_offset += head_dim;
                }
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    float pos_sum = 0;
                    for (int subindex_ = 0; subindex_ < head_dim * heads; subindex_++){
                        pos_sum += concatenated[subindex_] * output_weights->param[subindex * (head_dim * heads) + subindex_];
                    }
                    output_vector[subindex] = pos_sum + output_biases->param[subindex];
                }
                memcpy(combined_vectors[index], output_vector, embeddingSize * sizeof(float));
            }
            free(output_vector);
            free(concatenated);
            if (return_cache){
                free(head_outputs);
            }

        if (return_cache && antiOverfittingOptimisations){ //cache = training
            float dropout_rate = 0.1f;
            rets.cache.layers[layer].attn_dropout_mask = malloc(seq_len * sizeof(float*));
            if (!rets.cache.layers[layer].attn_dropout_mask){
                    failed_alloc("Failed to allocate memory to store attention dropout mask.");
                }
                for (int index = 0; index < seq_len; index++){
                    rets.cache.layers[layer].attn_dropout_mask[index] = malloc(embeddingSize * sizeof(float));
                    if (!rets.cache.layers[layer].attn_dropout_mask[index]){
                        failed_alloc("Failed to allocate memory to store attention dropout mask.");
                    }
                    for (int subindex = 0; subindex < embeddingSize; subindex++){
                        float range[] = {0, 0.99999994f};
                        float random_n = random_range(range);
                        float mask_val = (random_n < dropout_rate) ? 0.0f : (1.0f / (1.0f - dropout_rate));
                        rets.cache.layers[layer].attn_dropout_mask[index][subindex] = mask_val;
                        combined_vectors[index][subindex] *= mask_val;
                    }
                }
            }

            for (int index = 0; index < seq_len; index++){
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    float residual_scale = 0.70710678f;
                    combined_vectors[index][subindex] = (combined_vectors[index][subindex] + final_embeddings[index][subindex]) * residual_scale;
                }
            }

            if (return_cache){
                rets.cache.layers[layer].combined = combined_vectors;
            }

            param* norm2_weights = &layers[layer].weights.normalize_2;
            param* norm2_biases = &layers[layer].biases.normalize_2;
            float** normalized_vectors = NULL;
            if (return_cache){
                normalized_vectors = malloc(seq_len * sizeof(float*));
                if (!normalized_vectors){
                    failed_alloc("Failed to allocate memory to store normalized vectors.");
                }
                rets.cache.layers[layer].norm2_inv_rms = malloc(seq_len * sizeof(float));
                if (!rets.cache.layers[layer].norm2_inv_rms){
                    failed_alloc("Failed to allocate memory to store norm2 inv rms.");
                }
            }
            else{
                normalized_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to store normalized vectors.");
            }

            if (return_cache){
                rets.cache.layers[layer].norm2_x_hat = malloc(seq_len * sizeof(float*));
                if (!rets.cache.layers[layer].norm2_x_hat){
                    failed_alloc("Failed to allocate memory to store norm 2 x hats.");
                }
            }

            for (int index = 0; index < seq_len; index++){
                if (return_cache){
                    float* norm2_x_hat = _calculate_x_hat_only(combined_vectors[index], embeddingSize);
                    if (!norm2_x_hat){
                        printf("Failed to compute norm 2 x hat.\n");
                        exit(1);
                    }
                    rets.cache.layers[layer].norm2_x_hat[index] = norm2_x_hat;
                    float inv_rms_2 = calculate_inv_rms(combined_vectors[index], embeddingSize);
                    rets.cache.layers[layer].norm2_inv_rms[index] = inv_rms_2;
                }

                float* final_norm2 = NULL;
                if (return_cache){
                    final_norm2 = normalize_vector(combined_vectors[index], embeddingSize, norm2_weights, norm2_biases);
                    if (!final_norm2){
                        failed_alloc("Failed to compute norm 2.");
                    }
                }
                else{
                    final_norm2 = track(normalize_vector(combined_vectors[index], embeddingSize, norm2_weights, norm2_biases), "Failed to compute norm 2.");
                }

                normalized_vectors[index] = final_norm2;
            }

            if (return_cache){
                rets.cache.layers[layer].norm2_output = normalized_vectors;
            }

            float** bigger_vectors = NULL;
            float** gate_vectors = NULL;
            if (return_cache){
                bigger_vectors = malloc(seq_len * sizeof(float*));
                gate_vectors = malloc(seq_len * sizeof(float*));
                if (!bigger_vectors){
                    failed_alloc("Failed to allocate memory to compute ffw grow.");
                }
                if (!gate_vectors){
                    failed_alloc("Failed to allocate memory to compute ffw gate.");
                }
            }
            else{
                bigger_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute ffw grow.");
                gate_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute ffw gate.");
            }

            if (return_cache){
                for (int index = 0; index < seq_len; index++){
                    bigger_vectors[index] = calloc(embeddingSize * ffnGrowSize * sizeof(float), 1);
                    if (!bigger_vectors[index]){
                        failed_alloc("Failed to allocate memory to compute ffw grow.");
                    }
                    gate_vectors[index] = calloc(embeddingSize * ffnGrowSize * sizeof(float), 1);
                    if (!gate_vectors[index]){
                        failed_alloc("Failed to allocate memory to compute ffw gate.");
                    }
                }
            }
            else{
                for (int index = 0; index < seq_len; index++){
                    bigger_vectors[index] = track(calloc(embeddingSize * ffnGrowSize * sizeof(float), 1), "Failed toa llocate memory to compute ffw grow.");
                    gate_vectors[index] = track(calloc(embeddingSize * ffnGrowSize * sizeof(float), 1), "Failed toa llocate memory to compute ffw gate.");
                }
            }

            param* grow_weights = &layers[layer].weights.feed_forward.grow;
            param* grow_biases = &layers[layer].biases.feed_forward.grow;
            param* gate_weights = &layers[layer].weights.feed_forward.gate;
            param* gate_biases = &layers[layer].biases.feed_forward.gate;
            for (int index = 0; index < seq_len; index++){
                for (int subindex = 0; subindex < embeddingSize * ffnGrowSize; subindex++){
                    float sum_val = 0;
                    float gate_sum = 0;
                    for (int subindex_ = 0; subindex_ < embeddingSize; subindex_++){
                        sum_val += normalized_vectors[index][subindex_] * grow_weights->param[subindex_ * (embeddingSize * ffnGrowSize) + subindex];
                        gate_sum += normalized_vectors[index][subindex_] * gate_weights->param[subindex_ * (embeddingSize * ffnGrowSize) + subindex];
                    }
                    bigger_vectors[index][subindex] = sum_val + grow_biases->param[subindex];
                    gate_vectors[index][subindex] = gate_sum + gate_biases->param[subindex];
                }
            }

            if (return_cache){
                rets.cache.layers[layer].feed_forward.bigger = bigger_vectors;
            }

            float** after_relu_vectors = gate_vectors; //store gate pre-activations
            if (return_cache){
                rets.cache.layers[layer].feed_forward.after_relu = after_relu_vectors;
            }

            float** final_big_vectors = NULL;
            if (return_cache){
                final_big_vectors = malloc(seq_len * sizeof(float*));
                if (!final_big_vectors){
                    failed_alloc("Failed to allocate memory to compute fused ffw output.");
                }
            }
            else{
                final_big_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute fused ffw output.");
            }

            for (int index = 0; index < seq_len; index++){
                if (return_cache){
                    final_big_vectors[index] = calloc(embeddingSize * ffnGrowSize * sizeof(float), 1);
                    if (!final_big_vectors[index]){
                        failed_alloc("Failed to allocate memory to compute fused ffw output.");
                    }
                }
                else{
                    final_big_vectors[index] = track(calloc(embeddingSize * ffnGrowSize * sizeof(float), 1), "Failed to allocate memory to compute fused ffw output.");
                }
            }

            if (return_cache && antiOverfittingOptimisations){
                float dropout_rate = 0.1f;
                rets.cache.layers[layer].feed_forward.ff_dropout_mask = malloc(seq_len * sizeof(float*));
                if (!rets.cache.layers[layer].feed_forward.ff_dropout_mask){
                    failed_alloc("Failed to allocate memory to store ff dropout mask.");
                }
                for (int index = 0; index < seq_len; index++){
                    rets.cache.layers[layer].feed_forward.ff_dropout_mask[index] = malloc(embeddingSize * ffnGrowSize * sizeof(float));
                    if (!rets.cache.layers[layer].feed_forward.ff_dropout_mask[index]){
                        failed_alloc("Failed to allocate memory to store ff dropout mask.");
                    }
                    for (int subindex = 0; subindex < embeddingSize * ffnGrowSize; subindex++){
                        float gate_pre = gate_vectors[index][subindex];
                        float sig = sigmoid_scalar(gate_pre);
                        float gate_act = gate_pre * sig;
                        float fused = bigger_vectors[index][subindex] * gate_act;
                        float range[] = {0, 0.99999994f};
                        float random_n = random_range(range);
                        float mask_val = (random_n < dropout_rate) ? 0.0f : (1.0f / (1.0f - dropout_rate));
                        rets.cache.layers[layer].feed_forward.ff_dropout_mask[index][subindex] = mask_val;
                        final_big_vectors[index][subindex] = fused * mask_val;
                    }
                }
            }
            else{
                for (int index = 0; index < seq_len; index++){
                    for (int subindex = 0; subindex < embeddingSize * ffnGrowSize; subindex++){
                        float gate_pre = gate_vectors[index][subindex];
                        float sig = sigmoid_scalar(gate_pre);
                        float gate_act = gate_pre * sig;
                        float fused = bigger_vectors[index][subindex] * gate_act;
                        final_big_vectors[index][subindex] = fused;
                    }
                }
            }

            float** final_vectors = NULL;
            if (return_cache){
                final_vectors = malloc(seq_len * sizeof(float*));
                if (!final_vectors){
                    failed_alloc("Failed to allocate memory to compute ffw shrink.");
                }
            }
            else{
                final_vectors = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute ffw shrink.");
            }

            if (return_cache){
                for (int index = 0; index < seq_len; index++){
                    final_vectors[index] = calloc(embeddingSize * sizeof(float), 1);
                    if (!final_vectors[index]){
                        failed_alloc("Failed to allocate memory to compute ffw shrink.");
                    }
                }
            }
            else{
                for (int index = 0; index < seq_len; index++){
                    final_vectors[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute ffw shrink.");
                }
            }

            param* shrink_weights = &layers[layer].weights.feed_forward.shrink;
            param* shrink_biases = &layers[layer].biases.feed_forward.shrink;

            for (int index = 0; index < seq_len; index++){
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    float accum = 0;
                    for (int subindex_ = 0; subindex_ < embeddingSize * ffnGrowSize; subindex_++){
                        accum += final_big_vectors[index][subindex_] * shrink_weights->param[subindex * (embeddingSize * ffnGrowSize) + subindex_];
                    }
                    final_vectors[index][subindex] = accum + shrink_biases->param[subindex];
                }
            }

            for (int index = 0; index < seq_len; index++){
                float* residual = combined_vectors[index];
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    float residual_scale = 0.70710678f;
                    final_vectors[index][subindex] = (final_vectors[index][subindex] + residual[subindex]) * residual_scale;
                }
            }

            if (return_cache){
                rets.cache.layers[layer].feed_forward.final = final_vectors;
            }
            
            final_embeddings = final_vectors; //Output of this layer becomes input for the next
            verbprintf("Computed layer %d/%d in %lldms.\n", layer + 1, layersAmount, timer_end(layertimer));
        }
        verbprintf("Computed all (%d) layers in %lldms.\n", layersAmount, timer_end(layerstimer));

        if (return_cache){
            for (int index = 0; index < seq_len; index++){
                free(rope_cos[index]);
                free(rope_sin[index]);
            }
            free(rope_cos);
            free(rope_sin);
        }
        long long timer_token = timer();
        verbprintf("Computing next token...\n");

        if (seq_len == 0){
            printf("Cannot compute next token: sequence length is zero.\n");
            rets.success = false;
            cleanup();
            return rets;
        }

        float** scores = NULL;
        if (return_cache){
            scores = malloc(seq_len * sizeof(float*));
            if (!scores){
                failed_alloc("Failed to allocate memory to compute next token.");
            }
        }
        else{
            scores = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute next token.");
        }

        param* vocab_weights = &vocab_projection.weights;
        param* vocab_biases = &vocab_projection.biases;

        int start_pos = return_cache ? 0 : seq_len - 1;
        for (int pos = start_pos; pos < seq_len; pos++){
            if (return_cache){
                scores[pos] = calloc(vocab_len * sizeof(float), 1);
                if (!scores[pos]){
                    failed_alloc("Failed to allocate memory to compute next token.");
                }
            }
            else{
                scores[pos] = track(calloc(vocab_len * sizeof(float), 1), "Failed to allocate memory to compute next token.");
            }
            float* token_emb = final_embeddings[pos];
            for (int index = 0; index < vocab_len; index++){
                float score = 0;
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    score += token_emb[subindex] * vocab_weights->param[index * embeddingSize + subindex];
                }
                score += vocab_biases->param[index];
                scores[pos][index] = score;
            }
        }

        if (return_cache){
            rets.cache.vocab_scores = scores;
        }

        if (temperature < 0.0001){
            float highest_score = -FLT_MAX;
            int next_token_index = 0;
            for (int index = 0; index < vocab_len; index++){
                float score = scores[seq_len - 1][index];
                if (score > highest_score){
                    highest_score = score;
                    next_token_index = index;
                }
            }

            rets.predicted_token_id = next_token_index;
        }
        else{
            float* scaled_scores = track(calloc(vocab_len * sizeof(float), 1), "Failed to allocate memory to apply temperature to inference prediction.");
            for (int index = 0; index < vocab_len; index++){
                scaled_scores[index] = scores[seq_len - 1][index] / temperature;
            }
            float* probs = track(calloc(vocab_len * sizeof(float), 1), "Failed to allocate memory to apply temperature to inference prediction.");
            probs = softmax(scaled_scores, vocab_len, probs);
            float range[] = {0, 0.99999994f};
            float random_n = random_range(range);
            float cumulative_prob = 0;
            int next_token_index = 0;
            for (int index = 0; index < vocab_len; index++){
                cumulative_prob += probs[index];
                if (random_n <= cumulative_prob){
                    next_token_index = index;
                    break;
                }
            }

            rets.predicted_token_id = next_token_index;
        }

        verbprintf("Computed next token in %lldms.\n", timer_end(timer_token));
        if (return_cache){
            rets.cache.final_embeddings = final_embeddings;
        }
        else{
            for (int index = 0; index < seq_len; index++){
                untrack(final_embeddings[index]);
                free(final_embeddings[index]);
            }
            untrack(final_embeddings);
            free(final_embeddings);
        }
        cleanup();
        verbprintf("Did inference in %lldms.\n", timer_end(timer_));
        return rets;
    }

    typedef struct {
        char* token;
        int token_id;
    } train_step_token;

    typedef struct {
        float** embedding_grads;
        int* tokenized;
        layer* layer_grads;
        struct {
            param weights;
            param biases;
        } vocab_projection;
        float initial_loss;
        int seq_len;
        bool success;
    } train_step_ret;

    void free_train_step_ret(train_step_ret ret){
        int seq_len = ret.seq_len;
        if (ret.tokenized){
            free(ret.tokenized);
        }
        for (int index = 0; index < seq_len; index++){
            free(ret.embedding_grads[index]);
        }
        free(ret.embedding_grads);

        for (int index = 0; index < layersAmount; index++){
            free(ret.layer_grads[index].weights.normalize_1.param);
            free(ret.layer_grads[index].weights.normalize_1.m);
            free(ret.layer_grads[index].weights.normalize_1.v);
            free(ret.layer_grads[index].weights.normalize_2.param);
            free(ret.layer_grads[index].weights.normalize_2.m);
            free(ret.layer_grads[index].weights.normalize_2.v);
            free(ret.layer_grads[index].weights.attention.output.param);
            free(ret.layer_grads[index].weights.attention.output.m);
            free(ret.layer_grads[index].weights.attention.output.v);
            free(ret.layer_grads[index].weights.feed_forward.grow.param);
            free(ret.layer_grads[index].weights.feed_forward.grow.m);
            free(ret.layer_grads[index].weights.feed_forward.grow.v);
            free(ret.layer_grads[index].weights.feed_forward.gate.param);
            free(ret.layer_grads[index].weights.feed_forward.gate.m);
            free(ret.layer_grads[index].weights.feed_forward.gate.v);
            free(ret.layer_grads[index].weights.feed_forward.shrink.param);
            free(ret.layer_grads[index].weights.feed_forward.shrink.m);
            free(ret.layer_grads[index].weights.feed_forward.shrink.v);

            for (int subindex = 0; subindex < heads; subindex++){
                free(ret.layer_grads[index].weights.attention.heads[subindex].query.param);
                free(ret.layer_grads[index].weights.attention.heads[subindex].query.m);
                free(ret.layer_grads[index].weights.attention.heads[subindex].query.v);
                free(ret.layer_grads[index].weights.attention.heads[subindex].key.param);
                free(ret.layer_grads[index].weights.attention.heads[subindex].key.m);
                free(ret.layer_grads[index].weights.attention.heads[subindex].key.v);
                free(ret.layer_grads[index].weights.attention.heads[subindex].value.param);
                free(ret.layer_grads[index].weights.attention.heads[subindex].value.m);
                free(ret.layer_grads[index].weights.attention.heads[subindex].value.v);
            }
            free(ret.layer_grads[index].weights.attention.heads);

            free(ret.layer_grads[index].biases.normalize_1.param);
            free(ret.layer_grads[index].biases.normalize_1.m);
            free(ret.layer_grads[index].biases.normalize_1.v);
            free(ret.layer_grads[index].biases.normalize_2.param);
            free(ret.layer_grads[index].biases.normalize_2.m);
            free(ret.layer_grads[index].biases.normalize_2.v);
            free(ret.layer_grads[index].biases.attention.output.param);
            free(ret.layer_grads[index].biases.attention.output.m);
            free(ret.layer_grads[index].biases.attention.output.v);
            free(ret.layer_grads[index].biases.feed_forward.grow.param);
            free(ret.layer_grads[index].biases.feed_forward.grow.m);
            free(ret.layer_grads[index].biases.feed_forward.grow.v);
            free(ret.layer_grads[index].biases.feed_forward.gate.param);
            free(ret.layer_grads[index].biases.feed_forward.gate.m);
            free(ret.layer_grads[index].biases.feed_forward.gate.v);
            free(ret.layer_grads[index].biases.feed_forward.shrink.param);
            free(ret.layer_grads[index].biases.feed_forward.shrink.m);
            free(ret.layer_grads[index].biases.feed_forward.shrink.v);

            for (int subindex = 0; subindex < heads; subindex++){
                free(ret.layer_grads[index].biases.attention.heads[subindex].query.param);
                free(ret.layer_grads[index].biases.attention.heads[subindex].query.m);
                free(ret.layer_grads[index].biases.attention.heads[subindex].query.v);
                free(ret.layer_grads[index].biases.attention.heads[subindex].key.param);
                free(ret.layer_grads[index].biases.attention.heads[subindex].key.m);
                free(ret.layer_grads[index].biases.attention.heads[subindex].key.v);
                free(ret.layer_grads[index].biases.attention.heads[subindex].value.param);
                free(ret.layer_grads[index].biases.attention.heads[subindex].value.m);
                free(ret.layer_grads[index].biases.attention.heads[subindex].value.v);
            }
            free(ret.layer_grads[index].biases.attention.heads);
        }

        free(ret.layer_grads);

        free(ret.vocab_projection.weights.param);
        free(ret.vocab_projection.weights.m);
        free(ret.vocab_projection.weights.v);
        free(ret.vocab_projection.biases.param);
        free(ret.vocab_projection.biases.m);
        free(ret.vocab_projection.biases.v);
    }

    train_step_ret train_step(int* tokens, size_t tokens_len, train_step_token target_token){
        //I will be computing this once and only once for this function
        float scale = sqrtf(head_dim);

        //Memory tracking system
        void** tracked = NULL;
        size_t tracked_len = 0;

        int cmp_ptr(const void* a, const void* b){
            void* pa = *(void**)a;
            void* pb = *(void**)b;
            return (pa > pb) - (pa < pb); // or: return (uintptr_t)pa - (uintptr_t)pb;
        }

        void sort_tracked(){
            qsort(tracked, tracked_len, sizeof(void*), cmp_ptr);
        }

        ssize_t binary_search_tracked(void* key){
            size_t left = 0;
            size_t right = tracked_len;

            while (left < right) {
                size_t mid = left + (right - left) / 2;
                void* mid_ptr = tracked[mid];

                if (key == mid_ptr) {
                    return (ssize_t)mid;
                } else if (key < mid_ptr) {
                    right = mid;
                } else {
                    left = mid + 1;
                }
            }
            return -1;
        }

        //Failed alloc procedure.
        void failed_alloc(char* failed_task){
            printf("%s.\n", failed_task);
            exit(1); //let os reclaim memory lol
        }

        void* track(void* ptr, char* failed_task){
            if (!ptr){
                //Failed alloc = no memory = can't do anything anyways = should exit.
                failed_alloc(failed_task);
            }
            tracked_len++;
            void** tmp = realloc(tracked, tracked_len * sizeof(void*));
            if (!tmp){
                printf("Failed to allocate memory to update freelist.\n");
                tracked_len--;
                free(ptr);
                return NULL;
            }
            tracked = tmp;
            tracked[tracked_len - 1] = ptr;
            sort_tracked();
            return ptr;
        }

        void cleanup(){
            for (int index = 0; index < tracked_len; index++){
                free(tracked[index]);
            }
            free(tracked);
            tracked = NULL;
            tracked_len = 0;
            return;
        }

        bool untrack(void* ptr){
            ssize_t index_ptr = binary_search_tracked(ptr);
            if (index_ptr == -1){
                return false;
            }

            tracked_len--;
            void** new = malloc(tracked_len * sizeof(void*));
            if (!new){
                tracked_len++;
                return false;
            }

            bool offset = false;
            for (ssize_t index = 0; index < tracked_len + 1; index++){
                if (index == index_ptr){
                    offset = true;
                    continue;
                }
                if (!offset){
                    new[index] = tracked[index];
                }
                else{
                    new[index - 1] = tracked[index];
                }
            }

            free(tracked);
            tracked = new;
            return true;
        }

        void* notrack(void* ptr, char* failed_message){
            if (ptr){
                return ptr;
            }
            else{
                failed_alloc(failed_message);
                return NULL;
            }
        }

        train_step_ret rets = {0};
        rets.layer_grads = notrack(calloc(layersAmount, sizeof(layer)), "Failed to allocate memory to store layer gradients.");

        rets.success = true;
        if (head_dim < 1){
            printf("Head dimension is not initalized.\n");
            rets.success = false;
            return rets;
        }

        long long timer_ = timer();
        printf("Starting training step...\n");
        
        long long inference_cache_timer = timer();
        printf("Running inference to get cache.\n");
        infret inference_result = inference(tokens, tokens_len, true, 0, true);
        if (!inference_result.success){
            printf("Failed to get inference cache in %lldms.\n", timer_end(inference_cache_timer));
            printf("Failed train step in %lldms.\n", timer_end(timer_));
            rets.success = false;
            return rets;
        }
        printf("Got inference cache in %lldms.\n", timer_end(inference_cache_timer));
        
        typeof(((infret*)0)->cache) cache = inference_result.cache;

        int seq_len = cache.seq_len;
        rets.seq_len = seq_len;
        rets.tokenized = malloc(seq_len * sizeof(int));
        if (!rets.tokenized){
            printf("Failed to allocate memory to store token ids for gradients.\n");
            rets.success = false;
            free_cache_object(cache);
            cleanup();
            return rets;
        }
        memcpy(rets.tokenized, cache.tokenized, seq_len * sizeof(int));

        long long calculate_loss_timer = timer();
        printf("Calculating initial loss...\n");
        float total_loss = 0.0f;
        int loss_terms = 0;

        float epsilon_grad = antiOverfittingOptimisations ? 0.1f : 0.0f;

        float** predicted_probs = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute predicted probs.");
        float** initial_error = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute initial error.");

        for (int pos = 0; pos < seq_len; pos++){
            predicted_probs[pos] = track(calloc(vocab_len * sizeof(float), 1), "Failed to allocate memory to compute predicted probs.");
            initial_error[pos] = track(calloc(vocab_len * sizeof(float), 1), "Failed to allocate memory to compute initial error.");

            predicted_probs[pos] = softmax(cache.vocab_scores[pos], vocab_len, predicted_probs[pos]);

            int target_id;
            if (pos < seq_len - 1){
                target_id = cache.tokenized[pos + 1];
            }
            else{
                target_id = target_token.token_id;
            }

            for (int index = 0; index < vocab_len; index++){
                float target_val = epsilon_grad / (vocab_len - 1);
                if (index == target_id){
                    target_val = 1.0f - epsilon_grad;
                }
                total_loss += -target_val * logf(predicted_probs[pos][index] + 1e-12f);
                initial_error[pos][index] = predicted_probs[pos][index] - target_val;
            }
            loss_terms++;
        }

        float initial_loss = (loss_terms > 0) ? (total_loss / loss_terms) : 0.0f;
        rets.initial_loss = initial_loss;
        printf("Calculated initial loss (%.3f) in %lldms.\n", initial_loss, timer_end(calculate_loss_timer));
        
        printf("Computing gradients...\n");

        param vocab_projection_weight_gradients = {0};
        param vocab_projection_bias_gradients = {0};
        if (!alloc_param(&vocab_projection_weight_gradients, vocab_len * embeddingSize) ||
            !alloc_param(&vocab_projection_bias_gradients, vocab_len)){
            printf("Failed to allocate memory to compute vocabulary projection gradients.\n");
            rets.success = false;
            return rets;
        }

        float** error_gradients = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute error gradients.");
        for (int index = 0; index < seq_len; index++){
            error_gradients[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute error gradients.");
        }

        param* vocab_projection_weights = &vocab_projection.weights;
        float** final_layer_activation = cache.layers[layersAmount - 1].feed_forward.final; //[tokenized][embeddingSize]

        for (int pos = 0; pos < seq_len; pos++){
            float* activation = final_layer_activation[pos];
            for (int vocab_idx = 0; vocab_idx < vocab_len; vocab_idx++){
                float error_val = initial_error[pos][vocab_idx];
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    int weight_idx = vocab_idx * embeddingSize + subindex;
                    vocab_projection_weight_gradients.param[weight_idx] += error_val * activation[subindex];
                }
                vocab_projection_bias_gradients.param[vocab_idx] += error_val;
            }

            for (int emb = 0; emb < embeddingSize; emb++){
                float accum = 0;
                for (int vocab_idx = 0; vocab_idx < vocab_len; vocab_idx++){
                    accum += initial_error[pos][vocab_idx] * vocab_projection_weights->param[vocab_idx * embeddingSize + emb];
                }
                error_gradients[pos][emb] = accum;
            }
        }

        float** embedding_gradients = notrack(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute embedding gradients.");
        for (int index = 0; index < seq_len; index++){
            embedding_gradients[index] = notrack(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute embedding gradients.");
        }

        float** next_grad = error_gradients;
        
        for (int layer = layersAmount - 1; layer >= 0; layer--){
            if (!alloc_param(&rets.layer_grads[layer].weights.normalize_1, embeddingSize) ||
                !alloc_param(&rets.layer_grads[layer].weights.normalize_2, embeddingSize) ||
                !alloc_param(&rets.layer_grads[layer].biases.normalize_1, embeddingSize) ||
                !alloc_param(&rets.layer_grads[layer].biases.normalize_2, embeddingSize)){
                failed_alloc("Failed to allocate memory to compute layer gradients.");
            }
            rets.layer_grads[layer].weights.attention.heads = notrack(malloc(heads * sizeof(*rets.layer_grads[layer].weights.attention.heads)), "Failed to allocate memory to compute layer gradients.");
            rets.layer_grads[layer].biases.attention.heads = notrack(malloc(heads * sizeof(*rets.layer_grads[layer].biases.attention.heads)), "Failed to allocate memory to compute layer gradients.");

            if (!alloc_param(&rets.layer_grads[layer].weights.attention.output, embeddingSize * (head_dim * heads)) ||
                !alloc_param(&rets.layer_grads[layer].biases.attention.output, embeddingSize) ||
                !alloc_param(&rets.layer_grads[layer].weights.feed_forward.grow, embeddingSize * (embeddingSize * ffnGrowSize)) ||
                !alloc_param(&rets.layer_grads[layer].weights.feed_forward.gate, embeddingSize * (embeddingSize * ffnGrowSize)) ||
                !alloc_param(&rets.layer_grads[layer].weights.feed_forward.shrink, embeddingSize * (embeddingSize * ffnGrowSize)) ||
                !alloc_param(&rets.layer_grads[layer].biases.feed_forward.grow, embeddingSize * ffnGrowSize) ||
                !alloc_param(&rets.layer_grads[layer].biases.feed_forward.gate, embeddingSize * ffnGrowSize) ||
                !alloc_param(&rets.layer_grads[layer].biases.feed_forward.shrink, embeddingSize)){
                failed_alloc("Failed to allocate memory to compute layer gradients.");
            }

            for (int head = 0; head < heads; head++){
                if (!alloc_param(&rets.layer_grads[layer].weights.attention.heads[head].query, head_dim * embeddingSize) ||
                    !alloc_param(&rets.layer_grads[layer].weights.attention.heads[head].key, head_dim * embeddingSize) ||
                    !alloc_param(&rets.layer_grads[layer].weights.attention.heads[head].value, head_dim * embeddingSize) ||
                    !alloc_param(&rets.layer_grads[layer].biases.attention.heads[head].query, head_dim) ||
                    !alloc_param(&rets.layer_grads[layer].biases.attention.heads[head].key, head_dim) ||
                    !alloc_param(&rets.layer_grads[layer].biases.attention.heads[head].value, head_dim)){
                    failed_alloc("Failed to allocate memory to compute layer gradients.");
                }
            }

            float residual_scale = 0.70710678f;
            param* ffw_shrink_weights = &layers[layer].weights.feed_forward.shrink;
            float** grad_into_ffn_shrink = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to scale residual gradients.");
            float** grad_into_ffn_residual = grad_into_ffn_shrink;
            for (int index = 0; index < seq_len; index++){
                grad_into_ffn_shrink[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to scale residual gradients.");
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    grad_into_ffn_shrink[index][subindex] = next_grad[index][subindex] * residual_scale;
                }
            }

            float** fused_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute fused gradient.");
            for (int index = 0; index < seq_len; index++){
                fused_grad[index] = track(calloc(embeddingSize * ffnGrowSize * sizeof(float), 1), "Failed to allocate memory to compute fused gradient.");
                float* gate_pre_cache = cache.layers[layer].feed_forward.after_relu[index]; //gate pre-activations
                float* grow_cache = cache.layers[layer].feed_forward.bigger[index];
                for (int subindex = 0; subindex < embeddingSize * ffnGrowSize; subindex++){
                    float gate_pre = gate_pre_cache[subindex];
                    float sig = sigmoid_scalar(gate_pre);
                    float gate_act = gate_pre * sig;
                    float fused_val = grow_cache[subindex] * gate_act;
                    float mask_val = 1.0f;
                    if (cache.layers[layer].feed_forward.ff_dropout_mask){
                        mask_val = cache.layers[layer].feed_forward.ff_dropout_mask[index][subindex];
                    }
                    for (int subindex_ = 0; subindex_ < embeddingSize; subindex_++){
                        fused_grad[index][subindex] += grad_into_ffn_shrink[index][subindex_] * ffw_shrink_weights->param[subindex_ * (embeddingSize * ffnGrowSize) + subindex];
                        rets.layer_grads[layer].weights.feed_forward.shrink.param[subindex_ * (embeddingSize * ffnGrowSize) + subindex] += grad_into_ffn_shrink[index][subindex_] * fused_val * mask_val;
                    }
                }
            }
            if (cache.layers[layer].feed_forward.ff_dropout_mask){
                for (int index = 0; index < seq_len; index++){
                    for (int subindex = 0; subindex < embeddingSize * ffnGrowSize; subindex++){
                        fused_grad[index][subindex] *= cache.layers[layer].feed_forward.ff_dropout_mask[index][subindex];
                    }
                }
            }

            for (int index = 0; index < embeddingSize; index++){
                float bias_grad_sum = 0;
                for (int subindex = 0; subindex < seq_len; subindex++){
                    bias_grad_sum += grad_into_ffn_shrink[subindex][index];
                }
                rets.layer_grads[layer].biases.feed_forward.shrink.param[index] += bias_grad_sum;
            }

            param* ffw_grow_weights = &layers[layer].weights.feed_forward.grow;
            param* ffw_gate_weights = &layers[layer].weights.feed_forward.gate;
            float** grow_back = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute grow backprop.");
            float** gate_back = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute gate backprop.");
            for (int index = 0; index < seq_len; index++){
                grow_back[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute grow backprop.");
                gate_back[index] = track(calloc(embeddingSize * ffnGrowSize * sizeof(float), 1), "Failed to allocate memory to compute gate backprop.");
                float* norm2_output_cache = cache.layers[layer].norm2_output[index];
                float* gate_pre_cache = cache.layers[layer].feed_forward.after_relu[index];
                float* grow_cache = cache.layers[layer].feed_forward.bigger[index];
                for (int subindex = 0; subindex < embeddingSize * ffnGrowSize; subindex++){
                    float gate_pre = gate_pre_cache[subindex];
                    float sig = sigmoid_scalar(gate_pre);
                    float gate_act = gate_pre * sig;
                    float fused_grad_val = fused_grad[index][subindex];

                    float d_gate_act = fused_grad_val * grow_cache[subindex];
                    float silu_deriv = sig + gate_pre * sig * (1.0f - sig);
                    float gate_pre_grad = d_gate_act * silu_deriv;
                    gate_back[index][subindex] = gate_pre_grad;

                    float d_grow = fused_grad_val * gate_act;
                    for (int subindex_ = 0; subindex_ < embeddingSize; subindex_++){
                        grow_back[index][subindex_] += d_grow * ffw_grow_weights->param[subindex_ * (embeddingSize * ffnGrowSize) + subindex];
                        rets.layer_grads[layer].weights.feed_forward.grow.param[subindex_ * (embeddingSize * ffnGrowSize) + subindex] += d_grow * norm2_output_cache[subindex_];

                        rets.layer_grads[layer].weights.feed_forward.gate.param[subindex_ * (embeddingSize * ffnGrowSize) + subindex] += gate_pre_grad * norm2_output_cache[subindex_];
                    }
                    rets.layer_grads[layer].biases.feed_forward.grow.param[subindex] += d_grow;
                    rets.layer_grads[layer].biases.feed_forward.gate.param[subindex] += gate_pre_grad;
                }
            }

            //Combine gradients from grow and gate paths into norm2 input
            float** combined_norm2_back = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory for combined norm2 backprop.");
            for (int index = 0; index < seq_len; index++){
                combined_norm2_back[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory for combined norm2 backprop.");
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    combined_norm2_back[index][subindex] += grow_back[index][subindex];
                }
                for (int subindex_ = 0; subindex_ < embeddingSize * ffnGrowSize; subindex_++){
                    for (int subindex = 0; subindex < embeddingSize; subindex++){
                        combined_norm2_back[index][subindex] += gate_back[index][subindex_] * ffw_gate_weights->param[subindex * (embeddingSize * ffnGrowSize) + subindex_];
                    }
                }
            }

            param* norm2_weights = &layers[layer].weights.normalize_2;
            param* norm2_biases = &layers[layer].biases.normalize_2;
            float** norm2_output_grad = combined_norm2_back;
            float** grad_into_norm2_input = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute grad flowing into normalize 2 input.");
            for (int index = 0; index < seq_len; index++){
                grad_into_norm2_input[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute grad flowing into normalize 2 input.");
            }

            for (int index = 0; index < seq_len; index++){
                float mean_dxhat_xhat = 0;
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    float x_hat_val_2 = cache.layers[layer].norm2_x_hat[index][subindex];

                    rets.layer_grads[layer].weights.normalize_2.param[subindex] += norm2_output_grad[index][subindex] * x_hat_val_2;
                    rets.layer_grads[layer].biases.normalize_2.param[subindex] += norm2_output_grad[index][subindex];
                    float gamma2_val = norm2_weights->param[subindex];
                    float dxhat = norm2_output_grad[index][subindex] * gamma2_val;
                    mean_dxhat_xhat += dxhat * x_hat_val_2;
                }
                mean_dxhat_xhat /= embeddingSize;
                float inv_rms_2 = cache.layers[layer].norm2_inv_rms[index];
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    float gamma2_val = norm2_weights->param[subindex];
                    float dxhat = norm2_output_grad[index][subindex] * gamma2_val;
                    grad_into_norm2_input[index][subindex] = (dxhat - cache.layers[layer].norm2_x_hat[index][subindex] * mean_dxhat_xhat) * inv_rms_2;
                }
            }

            float** combined_grad_before_norm2 = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute combined gradients before normalize 2.");
            for (int index = 0; index < seq_len; index++){
                combined_grad_before_norm2[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute combined gradients before normalize 2.");
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    combined_grad_before_norm2[index][subindex] = grad_into_norm2_input[index][subindex] + grad_into_ffn_residual[index][subindex];
                }
            }

            float** scaled_combined_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to scale combined gradients.");
            for (int index = 0; index < seq_len; index++){
                scaled_combined_grad[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to scale combined gradients.");
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    scaled_combined_grad[index][subindex] = combined_grad_before_norm2[index][subindex] * residual_scale;
                }
            }

            param* attention_output_weights = &layers[layer].weights.attention.output;
            float** attention_output_grad = scaled_combined_grad;
            if (cache.layers[layer].attn_dropout_mask){
                for (int index = 0; index < seq_len; index++){
                    for (int subindex = 0; subindex < embeddingSize; subindex++){
                        attention_output_grad[index][subindex] *= cache.layers[layer].attn_dropout_mask[index][subindex];
                    }
                }
            }
            float** attention_output_input_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute attention output input grad.");
            float* concatenated_input = track(malloc(head_dim * heads * sizeof(float)), "Failed to allocate memory to compute concatenated input.");
            for (int index = 0; index < seq_len; index++){
                memset(concatenated_input, 0, head_dim * heads * sizeof(float));
                attention_output_input_grad[index] = track(calloc(head_dim * heads * sizeof(float), 1), "Failed to allocate memory to compute attention output input grad.");

                int current_offset = 0;
                for (int head = 0; head < heads; head++){
                    memcpy(concatenated_input + current_offset, cache.layers[layer].heads[head].output[index], head_dim * sizeof(float));
                    current_offset += head_dim;
                }
                
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    for (int subindex_ = 0; subindex_ < head_dim * heads; subindex_++){
                        attention_output_input_grad[index][subindex_] += attention_output_grad[index][subindex] * attention_output_weights->param[(subindex * (head_dim * heads)) + subindex_];
                        float weight_grad_delta = attention_output_grad[index][subindex] * concatenated_input[subindex_];
                        rets.layer_grads[layer].weights.attention.output.param[(subindex * (head_dim * heads)) + subindex_] += weight_grad_delta;
                    }
                    rets.layer_grads[layer].biases.attention.output.param[subindex] += attention_output_grad[index][subindex];
                }
            }

            float*** head_input_grads = track(malloc(heads * sizeof(float**)), "Failed to allocate memory to compute head input gradients.");
            float** head_grad_from_output = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute head gradient from output.");
            float** v_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute v gradient.");
            float** q_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute q gradient.");
            float** k_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute k gradient.");
            float** attention_prob_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute attention probability gradient.");
            float** attention_score_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute attention score gradient.");
            for (int index = 0; index < seq_len; index++){
                head_grad_from_output[index] = track(malloc(head_dim * sizeof(float)), "Failed to allocate memory to compute head gradient from output.");
                v_grad[index] = track(malloc(head_dim * sizeof(float)), "Failed to allocate memory to compute v gradient.");
                q_grad[index] = track(malloc(head_dim * sizeof(float)), "Failed to allocate memory to compute q gradient.");
                k_grad[index] = track(malloc(head_dim * sizeof(float)), "Failed to allocate memory to compute k gradient.");
                attention_prob_grad[index] = track(malloc(seq_len * sizeof(float)), "Failed to allocate memory to compute attention probability gradient.");
                attention_score_grad[index] = track(malloc(seq_len * sizeof(float)), "Failed to allocate memory to compute attention score gradient.");
            }

            for (int head = 0; head < heads; head++){
                for (int index = 0; index < seq_len; index++){
                    memset(head_grad_from_output[index], 0, head_dim * sizeof(float));
                    memset(v_grad[index], 0, head_dim * sizeof(float));
                    memset(q_grad[index], 0, head_dim * sizeof(float));
                    memset(k_grad[index], 0, head_dim * sizeof(float));
                    memset(attention_prob_grad[index], 0, seq_len * sizeof(float));
                    memset(attention_score_grad[index], 0, seq_len * sizeof(float));
                }
                head_input_grads[head] = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute head input gradients.");
                for (int index = 0; index < seq_len; index++){
                    head_input_grads[head][index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute head input gradients.");
                }
                for (int index = 0; index < seq_len; index++) {
                    memcpy(head_grad_from_output[index], attention_output_input_grad[index] + head * head_dim, head_dim * sizeof(float));
                }

                param* query_weights = &layers[layer].weights.attention.heads[head].query;
                param* key_weights = &layers[layer].weights.attention.heads[head].key;
                param* value_weights = &layers[layer].weights.attention.heads[head].value;

                float** norm1_output_cache = cache.layers[layer].normalized;
                for (int index = 0; index < seq_len; index++){
                    for (int subindex = 0; subindex < head_dim; subindex++){
                        for (int subindex_ = 0; subindex_ < seq_len; subindex_++){
                            v_grad[subindex_][subindex] += head_grad_from_output[index][subindex] * cache.layers[layer].heads[head].attention_probs[index][subindex_];
                            attention_prob_grad[index][subindex_] += head_grad_from_output[index][subindex] * cache.layers[layer].heads[head].v_vectors[subindex_][subindex];
                        }
                    }

                    for (int subindex = 0; subindex < seq_len; subindex++){
                        float d_score = 0;
                        for (int subindex_ = 0; subindex_ < seq_len; subindex_++){
                            d_score += attention_prob_grad[index][subindex_] * cache.layers[layer].heads[head].attention_probs[index][subindex_] * ((subindex == subindex_ ? 1 : 0) - cache.layers[layer].heads[head].attention_probs[index][subindex]);
                        }
                        attention_score_grad[index][subindex] = d_score;
                    }

                    //Oh the cpu is gonna love that loop fusing
                    for (int subindex = 0; subindex < seq_len; subindex++){
                        if (subindex <= index){
                            float score_grad = attention_score_grad[index][subindex] / scale;
                            for (int subindex_ = 0; subindex_ < head_dim; subindex_++){
                                q_grad[index][subindex_] += score_grad * cache.layers[layer].heads[head].k_vectors[subindex][subindex_];
                                k_grad[subindex][subindex_] += score_grad * cache.layers[layer].heads[head].q_vectors[index][subindex_];
                            }
                        }
                    }

                    float* normalized_input_embedding = norm1_output_cache[index];
                    for (int subindex = 0; subindex < head_dim; subindex++){
                        for (int subindex_ = 0; subindex_ < embeddingSize; subindex_++){
                            rets.layer_grads[layer].weights.attention.heads[head].query.param[subindex * embeddingSize + subindex_] += q_grad[index][subindex] * normalized_input_embedding[subindex_];
                            rets.layer_grads[layer].weights.attention.heads[head].key.param[subindex * embeddingSize + subindex_] += k_grad[index][subindex] * normalized_input_embedding[subindex_];
                            rets.layer_grads[layer].weights.attention.heads[head].value.param[subindex * embeddingSize + subindex_] += v_grad[index][subindex] * normalized_input_embedding[subindex_];
                        }
                        rets.layer_grads[layer].biases.attention.heads[head].query.param[subindex] += q_grad[index][subindex];
                        rets.layer_grads[layer].biases.attention.heads[head].key.param[subindex] += k_grad[index][subindex];
                        rets.layer_grads[layer].biases.attention.heads[head].value.param[subindex] += v_grad[index][subindex];
                    }

                    for (int subindex = 0; subindex < embeddingSize; subindex++){
                        float grad_sum_k = 0;
                        for (int subindex_ = 0; subindex_ < head_dim; subindex_++){
                            grad_sum_k += q_grad[index][subindex_] * query_weights->param[subindex_ * embeddingSize + subindex];
                            grad_sum_k += k_grad[index][subindex_] * key_weights->param[subindex_ * embeddingSize + subindex];
                            grad_sum_k += v_grad[index][subindex_] * value_weights->param[subindex_ * embeddingSize + subindex];
                        }
                        head_input_grads[head][index][subindex] = grad_sum_k;
                    }
                }
            }
            
            float** total_attention_input_grad = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute total attention input gradient.");
            float** grad_into_norm1_input = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute gradient into normalize 1 input.");
            float** norm1_output_grad = total_attention_input_grad;
            param* normalize_1_weights = &layers[layer].weights.normalize_1;
            float** grad_into_previous_layer_output = track(malloc(seq_len * sizeof(float*)), "Failed to allocate memory to compute gradient into previous layer's output.");

            for (int index = 0; index < seq_len; index++){
                total_attention_input_grad[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute total attention input gradient.");
                grad_into_norm1_input[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute gradient into normalize 1 input.");
                grad_into_previous_layer_output[index] = track(calloc(embeddingSize * sizeof(float), 1), "Failed to allocate memory to compute gradient into previous layer's output.");
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    for (int head = 0; head < heads; head++){
                        total_attention_input_grad[index][subindex] += head_input_grads[head][index][subindex];
                    }
                }

                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    float x_hat_val_1 = cache.layers[layer].norm1_x_hat[index][subindex];

                    rets.layer_grads[layer].weights.normalize_1.param[subindex] += norm1_output_grad[index][subindex] * x_hat_val_1;
                    rets.layer_grads[layer].biases.normalize_1.param[subindex] += norm1_output_grad[index][subindex];
                    float gamma1_val = normalize_1_weights->param[subindex];
                    float dxhat = norm1_output_grad[index][subindex] * gamma1_val;
                    grad_into_norm1_input[index][subindex] = dxhat;
                }
                float mean_dxhat_xhat = 0;
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    mean_dxhat_xhat += grad_into_norm1_input[index][subindex] * cache.layers[layer].norm1_x_hat[index][subindex];
                }
                mean_dxhat_xhat /= embeddingSize;
                float inv_rms_1 = cache.layers[layer].norm1_inv_rms[index];
                for (int subindex = 0; subindex < embeddingSize; subindex++){
                    grad_into_norm1_input[index][subindex] = (grad_into_norm1_input[index][subindex] - cache.layers[layer].norm1_x_hat[index][subindex] * mean_dxhat_xhat) * inv_rms_1;
                    grad_into_previous_layer_output[index][subindex] = grad_into_norm1_input[index][subindex] + scaled_combined_grad[index][subindex];
                }
            }

            next_grad = grad_into_previous_layer_output;

            if (layer == 0){
                for (int index = 0; index < seq_len; index++){
                    for (int subindex = 0; subindex < embeddingSize; subindex++){
                        embedding_gradients[index][subindex] += next_grad[index][subindex];
                    }
                }
            }
        }

        rets.embedding_grads = embedding_gradients;
        rets.vocab_projection.weights = vocab_projection_weight_gradients;
        rets.vocab_projection.biases = vocab_projection_bias_gradients;
        printf("Computed gradients in %lldms.\n", timer_end(timer_));
        free_cache_object(cache);
        cleanup();
        return rets;
    }

    typedef struct {
        int* tokens;
        size_t tokens_len;
        train_step_token target_token;
    } threadData;

    THREAD_RETURN THREAD_CALL workerThread(void* arg){
        threadData* data_ptr = arg;
        threadData data = *data_ptr;
        free(data_ptr);

        train_step_ret* rets = malloc(sizeof(train_step_ret));
        if (!rets){
            printf("Failed to allocate memory to setup worker thread.\n");
            exit(1);
        }

        train_step_ret stack_ret = train_step(data.tokens, data.tokens_len, data.target_token);

        memcpy(rets, &stack_ret, sizeof(train_step_ret));

        free(data.tokens);

        return rets;
    }

    typedef struct {
        int* tokens;
        size_t tokens_len;
        train_step_token target;
    } worker_task;

    worker_task* worker_tasklist = NULL;
    int worker_tasklist_len = 0;

    void add_to_worker_tasklist(int* tokens, size_t tokens_len, train_step_token target){
        worker_tasklist_len++;
        worker_tasklist = realloc(worker_tasklist, worker_tasklist_len * sizeof(worker_task));
        if (!worker_tasklist){
            printf("Failed to allocate memory to add worker task to worker tasklist.\n");
            exit(1);
        }
        worker_tasklist[worker_tasklist_len - 1].target = target;
        worker_tasklist[worker_tasklist_len - 1].tokens = tokens;
        worker_tasklist[worker_tasklist_len - 1].tokens_len = tokens_len;
        return;
    }

    train_step_ret** worker_gradients = NULL;
    int worker_gradients_len = 0;

    Thread* worker_threads = NULL;
    int worker_threads_len = 0;

    void apply_worker_gradients(){
        if (worker_gradients_len < 1){
            return;
        }

        char* optimizer = NULL;
        if (do_train){
            optimizer = train_optimizer;
        }
        else{
            optimizer = pre_train_optimizer;
        }

        bool use_adam = true;
        bool use_sgdm = false;
        bool use_sgd = false;

        if (optimizer){
            if (strcmp(optimizer, "adam") == 0){
                use_adam = true;
                use_sgdm = false;
                use_sgd = false;
            }
            else{
                if (strcmp(optimizer, "sgd_momentum") == 0){
                    use_adam = false;
                    use_sgdm = true;
                    use_sgd = false;
                }
                else{
                    if (strcmp(optimizer, "sgd") == 0){
                        use_adam = false;
                        use_sgdm = false;
                        use_sgd = true;
                    }
                }
            }
        }

        float lr = (float)(learningRate);
        float inv_batch = 1.0f / (float)(worker_gradients_len);

        float loss_sum = 0.0f;
        int loss_count = 0;
        for (int index = 0; index < worker_gradients_len; index++){
            train_step_ret* grad = worker_gradients[index];
            if (!grad){
                continue;
            }
            if (!grad->success){
                continue;
            }
            loss_sum += grad->initial_loss;
            loss_count++;
        }

        if (loss_count > 0){
            float avg_loss = loss_sum / (float)(loss_count);
            if (avg_loss < lr_plateau_best_loss - 1e-6f){
                lr_plateau_best_loss = avg_loss;
                lr_plateau_counter = 0;
            }
            else{
                lr_plateau_counter++;
                if (lr_plateau_counter >= patience){
                    learningRate *= lr_reduce_amount;
                    lr_plateau_counter = 0;
                    printf("Reduced learning rate to %f after plateau (avg_loss=%.6f best=%.6f).\n", learningRate, avg_loss, lr_plateau_best_loss);
                }
            }
            lr = (float)(learningRate);
        }

        void ensure_moments(param* p, size_t count, bool need_m, bool need_v){
            if (need_m){
                if (!p->m){
                    p->m = calloc(count, sizeof(float));
                    if (!p->m){
                        printf("Failed to allocate memory for optimizer moments.\n");
                        exit(1);
                    }
                }
            }
            if (need_v){
                if (!p->v){
                    p->v = calloc(count, sizeof(float));
                    if (!p->v){
                        printf("Failed to allocate memory for optimizer moments.\n");
                        exit(1);
                    }
                }
            }
        }

        void update_param(param* p, float* grad_ptr, size_t count, float grad_scale){
            bool need_m = use_adam || use_sgdm;
            bool need_v = use_adam;
            ensure_moments(p, count, need_m, need_v);
            for (size_t subindex = 0; subindex < count; subindex++){
                float g = grad_ptr[subindex] * grad_scale;
                if (use_adam){
                    p->m[subindex] = adam_params.beta1 * p->m[subindex] + (1.0f - adam_params.beta1) * g;
                    p->v[subindex] = adam_params.beta2 * p->v[subindex] + (1.0f - adam_params.beta2) * g * g;
                    float m_hat = p->m[subindex] / (1.0f - powf(adam_params.beta1, adam_params.t));
                    float v_hat = p->v[subindex] / (1.0f - powf(adam_params.beta2, adam_params.t));
                    p->param[subindex] -= lr * m_hat / (sqrtf(v_hat) + adam_params.epsilon);
                }
                else{
                    if (use_sgdm){
                        p->m[subindex] = adam_params.beta1 * p->m[subindex] + g;
                        p->param[subindex] -= lr * p->m[subindex];
                    }
                    else{
                        p->param[subindex] -= lr * g;
                    }
                }
            }
        }

        if (use_adam){
            adam_params.t += 1;
        }
        step_num += 1;

        for (int index = 0; index < worker_gradients_len; index++){
            train_step_ret* grad = worker_gradients[index];
            if (!grad){
                continue;
            }
            if (!grad->success){
                // Don't call free_train_step_ret on failed results - they have uninitialized memory
                if (grad->layer_grads){
                    free(grad->layer_grads);
                }
                free(grad);
                continue;
            }
            float grad_scale = inv_batch;

            for (int layer = 0; layer < layersAmount; layer++){
                update_param(&layers[layer].weights.normalize_1, grad->layer_grads[layer].weights.normalize_1.param, embeddingSize, grad_scale);
                update_param(&layers[layer].weights.normalize_2, grad->layer_grads[layer].weights.normalize_2.param, embeddingSize, grad_scale);
                update_param(&layers[layer].biases.normalize_1, grad->layer_grads[layer].biases.normalize_1.param, embeddingSize, grad_scale);
                update_param(&layers[layer].biases.normalize_2, grad->layer_grads[layer].biases.normalize_2.param, embeddingSize, grad_scale);

                for (int head = 0; head < heads; head++){
                    update_param(&layers[layer].weights.attention.heads[head].query, grad->layer_grads[layer].weights.attention.heads[head].query.param, head_dim * embeddingSize, grad_scale);
                    update_param(&layers[layer].weights.attention.heads[head].key, grad->layer_grads[layer].weights.attention.heads[head].key.param, head_dim * embeddingSize, grad_scale);
                    update_param(&layers[layer].weights.attention.heads[head].value, grad->layer_grads[layer].weights.attention.heads[head].value.param, head_dim * embeddingSize, grad_scale);
                    update_param(&layers[layer].biases.attention.heads[head].query, grad->layer_grads[layer].biases.attention.heads[head].query.param, head_dim, grad_scale);
                    update_param(&layers[layer].biases.attention.heads[head].key, grad->layer_grads[layer].biases.attention.heads[head].key.param, head_dim, grad_scale);
                    update_param(&layers[layer].biases.attention.heads[head].value, grad->layer_grads[layer].biases.attention.heads[head].value.param, head_dim, grad_scale);
                }

                update_param(&layers[layer].weights.attention.output, grad->layer_grads[layer].weights.attention.output.param, embeddingSize * (head_dim * heads), grad_scale);
                update_param(&layers[layer].biases.attention.output, grad->layer_grads[layer].biases.attention.output.param, embeddingSize, grad_scale);

                update_param(&layers[layer].weights.feed_forward.grow, grad->layer_grads[layer].weights.feed_forward.grow.param, embeddingSize * (embeddingSize * ffnGrowSize), grad_scale);
                update_param(&layers[layer].weights.feed_forward.gate, grad->layer_grads[layer].weights.feed_forward.gate.param, embeddingSize * (embeddingSize * ffnGrowSize), grad_scale);
                update_param(&layers[layer].weights.feed_forward.shrink, grad->layer_grads[layer].weights.feed_forward.shrink.param, embeddingSize * (embeddingSize * ffnGrowSize), grad_scale);

                update_param(&layers[layer].biases.feed_forward.grow, grad->layer_grads[layer].biases.feed_forward.grow.param, embeddingSize * ffnGrowSize, grad_scale);
                update_param(&layers[layer].biases.feed_forward.gate, grad->layer_grads[layer].biases.feed_forward.gate.param, embeddingSize * ffnGrowSize, grad_scale);
                update_param(&layers[layer].biases.feed_forward.shrink, grad->layer_grads[layer].biases.feed_forward.shrink.param, embeddingSize, grad_scale);
            }

            update_param(&vocab_projection.weights, grad->vocab_projection.weights.param, vocab_len * embeddingSize, grad_scale);
            update_param(&vocab_projection.biases, grad->vocab_projection.biases.param, vocab_len, grad_scale);

            for (int index = 0; index < grad->seq_len; index++){
                int tok_id = -1;
                if (grad->tokenized){
                    tok_id = grad->tokenized[index];
                }
                if (tok_id < 0){
                    continue;
                }
                if (tok_id >= vocab_len){
                    continue;
                }
                update_param(&embeddings[tok_id], grad->embedding_grads[index], embeddingSize, grad_scale);
            }

            free_train_step_ret(*grad);
            free(grad);
        }

        free(worker_gradients);
        worker_gradients = NULL;
        worker_gradients_len = 0;
    }

    float flush_worker_tasklist(){
        if (worker_tasklist_len < 1){
            return -1;
        }
        
        float loss_sum = 0;
        size_t loss_n = 0;
        while (worker_tasklist_len > 0){
            //We copy from the end so we can just realloc down to shrink the list instead of rebuilding
            int lower_cpy_idx = (worker_tasklist_len > batchSize) ? worker_tasklist_len - batchSize : 0;
            int amountToCpy = (worker_tasklist_len > batchSize) ? batchSize : worker_tasklist_len;

            int previous_worker_gradients_len = worker_gradients_len;
            
            worker_gradients_len += amountToCpy;
            worker_gradients = realloc(worker_gradients, worker_gradients_len * sizeof(train_step_ret*));
            if (!worker_gradients){
                printf("Failed to allocate memory to store worker gradients.\n");
                exit(1);
            }

            int worker_write_idx = 0;
            for (int index = worker_tasklist_len - 1; index >= lower_cpy_idx; index--){
                threadData* data = malloc(sizeof(threadData));
                if (!data){
                    printf("Failed to allocate memory to store worker data.\n");
                    exit(1);
                }

                data->tokens = worker_tasklist[index].tokens;
                data->tokens_len = worker_tasklist[index].tokens_len;
                data->target_token = worker_tasklist[index].target;

                Thread thread;
                worker_threads_len++;
                worker_threads = realloc(worker_threads, worker_threads_len * sizeof(Thread));
                if (!worker_threads){
                    printf("Failed to allocate memory to store worker threads.\n");
                    exit(1);
                }
                thread_start(workerThread, data, thread);
                worker_threads[worker_write_idx] = thread;

                worker_write_idx++;
            }

            worker_write_idx = 0;
            for (int index = worker_tasklist_len - 1; index >= lower_cpy_idx; index--){
                train_step_ret* rets = 0;
                thread_join(worker_threads[worker_write_idx], &rets);

                worker_gradients[previous_worker_gradients_len + worker_write_idx] = rets;
                worker_write_idx++;
            }

            for (int index = 0; index < worker_gradients_len; index++){
                if (!worker_gradients[index]){
                    continue;
                }
                if (!worker_gradients[index]->success){
                    continue;
                }
                loss_sum += worker_gradients[index]->initial_loss;
                loss_n++;
            }

            free(worker_threads);
            worker_threads = NULL;
            worker_threads_len = 0;

            worker_tasklist_len -= amountToCpy;
            worker_tasklist = realloc(worker_tasklist, worker_tasklist_len * sizeof(worker_task));
            if (!worker_tasklist && worker_tasklist_len > 0){
                printf("Failed to allocate memory to store worker tasklist.\n");
                exit(1);
            }

            // Apply and free gradients after each batch
            apply_worker_gradients();
        }

        return loss_sum / (float)(loss_n);
    }

    float flush_worker_tasklist_if_required(){
        if (worker_tasklist_len >= batchSize){
            return flush_worker_tasklist();
        }
        else{
            return -1;
        }
    }

    volatile sig_atomic_t got_sigint = 0;
    volatile long long last_ctrl_c = 0;

    void handle_sigint(int sig) {
        if ((time_ms() - last_ctrl_c > 300) && (got_sigint == 1)){
            got_sigint = 0;
            if (time_ms() - last_ctrl_c < 1000){
                write(STDOUT_FILENO, "\nFaster 🫩\n", strlen("\nFaster 🫩\n"));
                return;
            }
            write(STDOUT_FILENO, "\n(Press ctrl + c again to exit)\n", strlen("\n(Press ctrl + c again to exit)\n"));
            return;
        }

        if (got_sigint){
            exit(130);
        }

        write(STDOUT_FILENO, "\n(Press ctrl + c again to exit)\n", strlen("\n(Press ctrl + c again to exit)\n"));
        got_sigint = 1;

        last_ctrl_c = time_ms();
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sa, NULL);

    bool training_interactive_cli(float current_loss, float* loss_history, size_t loss_history_len, int current_epoch, int total_epochs){
        printf("\n-----------------------------------------------------\n");
        printf("Training checkpoint - Epoch %d/%d\n", current_epoch, total_epochs);
        printf("Current loss: %.6f\n", current_loss);
        if (loss_history_len > 0){
            float best_loss = loss_history[0];
            int start = (loss_history_len > 5) ? loss_history_len - 5 : 0;
            printf("Loss history (last 5): ");

            for (int i = 0; i < loss_history_len; i++){
                if (loss_history[i] < best_loss){
                    best_loss = loss_history[i];
                }
                if (i >= start){
                    printf("%.6f ", loss_history[i]);
                }
            }
            printf("\nBest loss: %.6f\n", best_loss);
        }
        printf("Default temp to test the model is 0\n");
        printf("Commands:\n");
        printf("  /continue                  -- Continue training.\n");
        printf("  /stop                      -- Stop training.\n");
        printf("  /save <filename>           -- Save model checkpoint.\n");
        printf("  /temperature [temperature] -- Display/set current temperature.\n");
        printf("  /learningRate [lr]         -- Display/set current learning rate.\n");
        printf("  /batchSize [size]          -- Display/set current batch size.\n");
        printf("  /reset_context             -- Reset conversation context.\n");
        printf("\n");
        printf("Do you want to enable multiline inputs? This will allow you to send messages multiple lines long, however as enter will be used to create a new line, you will have to use ctrl + d twice to send your message/command instead of just pressing enter.\n");
        char* multilineornot = NULL;
        bool use_multiline = false;
        while (true){
            multilineornot = input("Enable multiline inputs? (y/n) ");
            if (!multilineornot){
                printf("Failed to read user input.\n");
                return false;
            }

            if (strcmp(multilineornot, "y") == 0){
                printf("Enabled multiline inputs.\n");
                printf("\n");
                free(multilineornot);
                multilineornot = NULL;
                use_multiline = true;
                break;
            }
            else{
                if (strcmp(multilineornot, "n") == 0){
                    printf("Disabled multiline inputs.\n");
                    printf("\n");
                    free(multilineornot);
                    multilineornot = NULL;
                    use_multiline = false;
                    break;
                }
                else{
                    printf("Invalid input, type 'y' for yes or 'n' for no.\n");
                    free(multilineornot);
                    multilineornot = NULL;
                    continue;
                }
            }
        }

        float temp = 0.0f;

        // Precompute template tokens
        tokenize_ret bos_system_tok = tokenize("<|bos|>\nSystem: ", false);
        if (!bos_system_tok.success){
            printf("Failed to tokenize bos_system template.\n");
            return false;
        }
        tokenize_ret person_tok = tokenize("\n\nPerson:\n", false);
        if (!person_tok.success){
            printf("Failed to tokenize person template.\n");
            free_tokenize_ret(bos_system_tok);
            return false;
        }
        tokenize_ret you_tok = tokenize("\nYou:\n", false);
        if (!you_tok.success){
            printf("Failed to tokenize you template.\n");
            free_tokenize_ret(bos_system_tok);
            free_tokenize_ret(person_tok);
            return false;
        }

        // Initialize context with bos_system tokens
        int* context_tokens = malloc(bos_system_tok.seq_len * sizeof(int));
        if (!context_tokens){
            printf("Failed to allocate memory to setup checkpoint CLI.\n");
            free_tokenize_ret(bos_system_tok);
            free_tokenize_ret(person_tok);
            free_tokenize_ret(you_tok);
            return false;
        }
        memcpy(context_tokens, bos_system_tok.tokens, bos_system_tok.seq_len * sizeof(int));
        size_t context_tokens_len = bos_system_tok.seq_len;
        
        while (true){
            printf("\nYou:\n");
            char* input_to_inference = use_multiline ? input_multiline("› ") : input("› ");
            if (!input_to_inference){
                printf("Failed to read user input.\n");
                return false;
            }

            size_t input_to_inference_len = strlen(input_to_inference);
            if (strcmp(input_to_inference, "/continue") == 0){
                free(input_to_inference);
                free(context_tokens);
                free_tokenize_ret(bos_system_tok);
                free_tokenize_ret(person_tok);
                free_tokenize_ret(you_tok);
                return false;
            }
            else if (strcmp(input_to_inference, "/stop") == 0){
                free(input_to_inference);
                free(context_tokens);
                free_tokenize_ret(bos_system_tok);
                free_tokenize_ret(person_tok);
                free_tokenize_ret(you_tok);
                return true;
            }
            else if (strcmp(input_to_inference, "/reset_context") == 0){
                free(input_to_inference);
                free(context_tokens);
                context_tokens = malloc(bos_system_tok.seq_len * sizeof(int));
                if (!context_tokens){
                    printf("Failed to allocate memory to reset context.\n");
                    free_tokenize_ret(bos_system_tok);
                    free_tokenize_ret(person_tok);
                    free_tokenize_ret(you_tok);
                    return false;
                }
                memcpy(context_tokens, bos_system_tok.tokens, bos_system_tok.seq_len * sizeof(int));
                context_tokens_len = bos_system_tok.seq_len;
                printf("Context has been reset.\n");
                continue;
            }
            else{
                if (input_to_inference_len >= strlen("/save x")){
                    char charaftersave = input_to_inference[strlen("/save ")];
                    input_to_inference[strlen("/save ")] = '\0';
                    if (strcmp(input_to_inference, "/save ") == 0){
                        input_to_inference[strlen("/save ")] = charaftersave;
                        char* newptr = input_to_inference + strlen("/save ");
                        save(newptr);
                        printf("Saved model as '%s'\n", newptr);
                        free(input_to_inference);
                        continue;
                    }
                    else{
                        input_to_inference[strlen("/save ")] = charaftersave;
                        if (input_to_inference_len >= strlen("/temperature x")){
                            charaftersave = input_to_inference[strlen("/temperature ")];
                            input_to_inference[strlen("/temperature ")] = '\0';
                            if (strcmp(input_to_inference, "/temperature ") == 0){
                                input_to_inference[strlen("/temperature ")] = charaftersave;
                                char* newptr = input_to_inference + strlen("/temperature ");
                                float ntemp = (float)(atof(newptr));
                                if (ntemp < 0){
                                    printf("Invalid temperature, must be float >=0.\n");
                                    free(input_to_inference);
                                    continue;
                                }
                                temp = ntemp;
                                printf("Temperature now set to %f.\n", temp);
                                free(input_to_inference);
                                continue;
                            }
                            else{
                                input_to_inference[strlen("/temperature ")] = charaftersave;
                            }
                        }
                        if (input_to_inference_len >= strlen("/learningRate x")){
                            charaftersave = input_to_inference[strlen("/learningRate ")];
                            input_to_inference[strlen("/learningRate ")] = '\0';
                            if (strcmp(input_to_inference, "/learningRate ") == 0){
                                input_to_inference[strlen("/learningRate ")] = charaftersave;
                                char* newptr = input_to_inference + strlen("/learningRate ");
                                double nlr = atof(newptr);
                                if (nlr < 0){
                                    printf("Invalid learning rate, must be >= 0.\n");
                                    free(input_to_inference);
                                    continue;
                                }
                                learningRate = nlr;
                                printf("Learning rate now set to %f.\n", learningRate);
                                free(input_to_inference);
                                continue;
                            }
                            else{
                                input_to_inference[strlen("/learningRate ")] = charaftersave;
                            }
                        }
                        if (input_to_inference_len >= strlen("/batchSize x")){
                            charaftersave = input_to_inference[strlen("/batchSize ")];
                            input_to_inference[strlen("/batchSize ")] = '\0';
                            if (strcmp(input_to_inference, "/batchSize ") == 0){
                                input_to_inference[strlen("/batchSize ")] = charaftersave;
                                char* newptr = input_to_inference + strlen("/batchSize ");
                                int nbs = atoi(newptr);
                                if (nbs <= 0){
                                    printf("Invalid batch size, must be > 0.\n");
                                    free(input_to_inference);
                                    continue;
                                }
                                batchSize = nbs;
                                printf("Batch size now set to %d.\n", batchSize);
                                free(input_to_inference);
                                continue;
                            }
                            else{
                                input_to_inference[strlen("/batchSize ")] = charaftersave;
                            }
                        }
                        if (input_to_inference_len >= strlen("/autosave x")){
                            charaftersave = input_to_inference[strlen("/autosave ")];
                            input_to_inference[strlen("/autosave ")] = '\0';
                            if (strcmp(input_to_inference, "/autosave ") == 0){
                                input_to_inference[strlen("/autosave ")] = charaftersave;
                                char* newptr = input_to_inference + strlen("/autosave ");
                                if (strcmp(newptr, "true") == 0 || strcmp(newptr, "on") == 0 || strcmp(newptr, "yes") == 0){
                                    autosave = true;
                                    printf("Autosave is now on.\n");
                                }
                                else if (strcmp(newptr, "false") == 0 || strcmp(newptr, "off") == 0 || strcmp(newptr, "no") == 0){
                                    autosave = false;
                                    printf("Autosave is now off.\n");
                                }
                                else{
                                    printf("Invalid autosave value. Use: true, on, yes, false, off, or no.\n");
                                }
                                free(input_to_inference);
                                continue;
                            }
                            else{
                                input_to_inference[strlen("/autosave ")] = charaftersave;
                            }
                        }
                        if (strcmp(input_to_inference, "/temperature") == 0){
                            printf("Current temperature: %.2f\n", temp);
                            printf("You can also specify a new temperature like this:\n");
                            printf("  /temperature <new_temperature>\n");
                            free(input_to_inference);
                            continue;
                        }
                        else if (strcmp(input_to_inference, "/learningRate") == 0){
                            printf("Current learning rate: %f\n", learningRate);
                            printf("You can also specify a new learning rate like this:\n");
                            printf("  /learningRate <new_learning_rate>\n");
                            free(input_to_inference);
                            continue;
                        }
                        else if (strcmp(input_to_inference, "/batchSize") == 0){
                            printf("Current batch size: %d\n", batchSize);
                            printf("You can also specify a new batch size like this:\n");
                            printf("  /batchSize <new_batch_size>\n");
                            free(input_to_inference);
                            continue;
                        }
                        else if (strcmp(input_to_inference, "/autosave") == 0){
                            printf("Autosave is %s\n", autosave ? "on" : "off");
                            printf("You can also specify a new autosave state like this:\n");
                            printf("  /autosave <true|on|yes|false|off|no>\n");
                            free(input_to_inference);
                            continue;
                        }
                        else{
                            goto checkpoint_generate_turn;
                        }
                    }
                }
                else{
                    if (strcmp(input_to_inference, "/save") == 0){
                        printf("You need to provide a filename with /save.\n");
                        printf("Usage: /save <filename>\n");
                        printf("(With <filename> being any valid filename)\n");
                        free(input_to_inference);
                        continue;
                    }
                    else if (strcmp(input_to_inference, "/temperature") == 0){
                        printf("Current temperature: %.2f\n", temp);
                        printf("You can also specify a new temperature like this:\n");
                        printf("  /temperature <new_temperature>\n");
                        free(input_to_inference);
                        continue;
                    }
                    else if (strcmp(input_to_inference, "/learningRate") == 0){
                        printf("Current learning rate: %f\n", learningRate);
                        printf("You can also specify a new learning rate like this:\n");
                        printf("  /learningRate <new_learning_rate>\n");
                        free(input_to_inference);
                        continue;
                    }
                    else if (strcmp(input_to_inference, "/batchSize") == 0){
                        printf("Current batch size: %d\n", batchSize);
                        printf("You can also specify a new batch size like this:\n");
                        printf("  /batchSize <new_batch_size>\n");
                        free(input_to_inference);
                        continue;
                    }
                    else if (strcmp(input_to_inference, "/autosave") == 0){
                        printf("Autosave is %s\n", autosave ? "on" : "off");
                        printf("You can also specify a new autosave state like this:\n");
                        printf("  /autosave <true|on|yes|false|off|no>\n");
                        free(input_to_inference);
                        continue;
                    }
                    else{
                        goto checkpoint_generate_turn;
                    }
                }
            }

        checkpoint_generate_turn:
            // Tokenize user input
            tokenize_ret input_tok = tokenize(input_to_inference, false);
            if (!input_tok.success){
                printf("Failed to tokenize user input.\n");
                free(input_to_inference);
                continue;
            }

            // Append person_tok + input_tok + you_tok to context
            size_t new_len = context_tokens_len + person_tok.seq_len + input_tok.seq_len + you_tok.seq_len;
            context_tokens = realloc(context_tokens, new_len * sizeof(int));
            if (!context_tokens){
                printf("Failed to allocate memory to track chat context.\n");
                free_tokenize_ret(input_tok);
                return false;
            }
            memcpy(context_tokens + context_tokens_len, person_tok.tokens, person_tok.seq_len * sizeof(int));
            context_tokens_len += person_tok.seq_len;
            memcpy(context_tokens + context_tokens_len, input_tok.tokens, input_tok.seq_len * sizeof(int));
            context_tokens_len += input_tok.seq_len;
            memcpy(context_tokens + context_tokens_len, you_tok.tokens, you_tok.seq_len * sizeof(int));
            context_tokens_len += you_tok.seq_len;
            free_tokenize_ret(input_tok);

            printf("Model:\n");
            long long generate_timer = timer();
            size_t token_n = 0;
            while (true){
                if (token_n + 1 > maxOutputSize){
                    printf("[Reached max output size]\n");
                    break;
                }
                if (context_tokens_len >= contextSize){
                    printf("[Context full]\n");
                    break;
                }
                infret rets = inference(context_tokens, context_tokens_len, false, temp, false);
                if (!rets.success){
                    printf("[Inference failure]\n");
                    break;
                }
                token_n++;
                int predicted_token_id = rets.predicted_token_id;
                char* predicted_token = id_to_token(predicted_token_id);
                bool eos = false;
                if (strcmp(predicted_token, "<|eos|>") == 0){
                    eos = true;
                    goto after_inference_testloop_print;
                }

                printf("%s", predicted_token);
                fflush(stdout);

            after_inference_testloop_print:
                // Append predicted token ID to context
                context_tokens = realloc(context_tokens, (context_tokens_len + 1) * sizeof(int));
                if (!context_tokens){
                    printf("Failed to allocate memory to track chat context.\n");
                    return false;
                }
                context_tokens[context_tokens_len] = predicted_token_id;
                context_tokens_len++;
                if (eos){
                    break;
                }
            }
            long long timerresult = timer_end(generate_timer);
            free(input_to_inference);
            float tok_per_s = 0;
            if (timerresult > 0){
                tok_per_s = 1000 * (float)(token_n) / (float)(timerresult);
            }
            printf("(Generated in %lldms, %d tokens, %.2f tok/sec avr.)\n", timerresult, (int)token_n, tok_per_s);
        }
    }

    void pretrain(int epochAmount){
        long long pretrain_timer = timer();
        printf("Starting pretraining...\n");
        
        float* loss_history = NULL;
        size_t loss_history_len = 0;

        for (int epoch = 0; epoch < epochAmount; epoch++){
            float loss_sum = 0;
            size_t loss_n = 0;
            long long epoch_timer = timer();
            printf("Starting epoch %d/%d...\n", epoch + 1, epochAmount);
            
            for (int dataset = 0; dataset < pre_training_paths_len; dataset++){
                long long dataset_timer = timer();
                printf("Pretraining using dataset %d/%d...\n", dataset + 1, pre_training_paths_len);

                long long dataset_read_timer = timer();
                printf("Reading dataset...\n");
                char* dataset_file = read_file(pre_training_paths[dataset]);
                if (!dataset_file){
                    printf("Failed to read dataset in %lldms.\n", timer_end(dataset_read_timer));
                    exit(1);
                }
                printf("Read dataset in %lldms.\n", timer_end(dataset_read_timer));

                printf("Tokenizing dataset...\n");
                tokenize_ret full_tokens = tokenize(dataset_file, true);
                free(dataset_file);
                
                if (!full_tokens.success){
                    printf("Failed to tokenize dataset.\n");
                    exit(1);
                }
                printf("Tokenized dataset (%zu tokens).\n", full_tokens.seq_len);

                size_t total_tokens = full_tokens.seq_len;
                int* all_tokens = full_tokens.tokens;

                long long batch_time_sums = 0;
                size_t batch_time_n = 0;
                int every_n_batch = 0;
                
                long long window_timer = timer();
                printf("Creating training tasks with sliding window (ctx=%d)...\n", contextSize);
                
                size_t tasks_created = 0;
                
                // For each token position, predict it from the previous tokens
                for (size_t pos = 1; pos < total_tokens; pos++){
                    // Grow from 1 token up to contextSize, then maintain contextSize
                    size_t ctx_start = (pos > contextSize) ? (pos - contextSize) : 0;
                    size_t ctx_len = pos - ctx_start;
                    
                    int* context_tokens = malloc(ctx_len * sizeof(int));
                    if (!context_tokens){
                        printf("Failed to allocate memory for context window.\n");
                        exit(1);
                    }
                    memcpy(context_tokens, all_tokens + ctx_start, ctx_len * sizeof(int));
                    
                    int target_token_id = all_tokens[pos];
                    train_step_token target = {0};
                    target.token = id_to_token(target_token_id);
                    target.token_id = target_token_id;
                    
                    add_to_worker_tasklist(context_tokens, ctx_len, target);
                    tasks_created++;
                    
                    long long timer_flush = timer();
                    float loss_sum_add = flush_worker_tasklist_if_required();
                    timer_flush = timer_end(timer_flush);
                    if (loss_sum_add != -1){
                        batch_time_sums += timer_flush;
                        batch_time_n++;
                        loss_sum += loss_sum_add;
                        loss_n++;
                        if (batch_time_n > 0){
                            long long eta_ms = batch_time_sums / batch_time_n * (int)((total_tokens -  pos) / batchSize);
                            int hours = eta_ms / 3600000;
                            int mins = (eta_ms % 3600000) / 60000;
                            int secs = (eta_ms % 60000) / 1000;
                            printf("\n------------------\n");
                            
                            if (eta_pause_toggle && every_n_batch == eta_pause_every_n_batch - 1){
                                printf("ETA for current dataset: %02dh%02dm%02ds\n", hours, mins, secs);
                                printf("(Pausing for %dms for you to see ETA)\n\n", eta_pause_time_ms);
                                sleep_ms(eta_pause_time_ms);
                                every_n_batch = 0;
                            }
                            else{
                                printf("ETA for current dataset: %02dh%02dm%02ds\n\n", hours, mins, secs);
                                every_n_batch++;
                            }
                        }
                        else{
                            printf("\n------------------\n");
                            printf("ETA for current dataset: Not enough data to estimate.\n\n");
                        }
                    }
                }
                printf("Created %zu training tasks in %lldms.\n", tasks_created, timer_end(window_timer));
                
                printf("Flushing remaining worker tasks...\n");
                long long timer_remaining = timer();
                float loss_avr_remaining = flush_worker_tasklist();
                if (loss_avr_remaining != -1){
                    loss_sum += loss_avr_remaining;
                    loss_n++;
                    timer_remaining = timer_end(timer_remaining);
                    batch_time_sums += timer_remaining;
                    batch_time_n++;
                    printf("Flushed remaining tasks with loss %f in %lldms.\n", loss_avr_remaining, timer_remaining);
                }
                else{
                    printf("There were no remaining worker tasks to flush (found out in %lldms).\n", timer_end(timer_remaining));
                }
                
                free(full_tokens.tokens);
                printf("Pretrained using dataset %d/%d in %lldms.\n", dataset + 1, pre_training_paths_len, timer_end(dataset_timer));
            }
            
            float loss_avr = (loss_n > 0) ? (loss_sum / (float)(loss_n)) : 0.0f;
            loss_history_len++;
            loss_history = realloc(loss_history, loss_history_len * sizeof(float));
            if (!loss_history){
                printf("Failed to allocate memory to track loss history.\n");
                exit(1);
            }
            loss_history[loss_history_len - 1] = loss_avr;
            printf("Finished epoch %d/%d with avg loss %.6f in %lldms.\n", epoch + 1, epochAmount, loss_avr, timer_end(epoch_timer));

            printf("Autosave is %s.\n", autosave ? "on" : "off");
            float current_loss = loss_avr;
            if (autosave){
                char filename[256];
                sprintf(filename, "model_%.6f.zip", current_loss);

                int modifier = 1;
                while (file_exists(filename)){
                    sprintf(filename, "model_%.6f_%d.zip", current_loss, modifier);
                    modifier++;
                }

                save(filename);
            }

            char* checkpoint_response = input_with_timeout("Do you want to open checkpoint cli? Write anything and press enter if so (30 seconds to answer): ", 30000);
            if (checkpoint_response){
                free(checkpoint_response);
                bool should_stop = training_interactive_cli(loss_avr, loss_history, loss_history_len, epoch + 1, epochAmount);
                if (should_stop){
                    break;
                }
            }
        }
        
        free(loss_history);
        printf("Finished pretraining for %d epochs in %lldms.\n", epochAmount, timer_end(pretrain_timer));
        return;
    }

    void train(int epochAmount){
        long long train_timer = timer();
        printf("Starting training...\n");
        
        float* loss_history = NULL;
        size_t loss_history_len = 0;

        for (int epoch = 0; epoch < epochAmount; epoch++){
            float loss_sum = 0;
            size_t loss_n = 0;
            long long epoch_timer = timer();
            printf("Starting epoch %d/%d...\n", epoch + 1, epochAmount);
            for (int dataset = 0; dataset < training_dataset_paths_len; dataset++){
                long long dataset_timer = timer();
                printf("Training using dataset %d/%d...\n", dataset + 1, training_dataset_paths_len);

                long long dataset_read_timer = timer();
                printf("Reading dataset...\n");
                char* dataset_file = read_file(training_dataset_paths[dataset]);
                if (!dataset_file){
                    printf("Failed to read dataset in %lldms.\n", timer_end(dataset_read_timer));
                    exit(1);
                }
                printf("Read dataset in %lldms.\n", timer_end(dataset_read_timer));

                long long dataset_parse_timer = timer();
                printf("Parsing dataset...\n");
                cJSON* dataset_raw = cJSON_Parse(dataset_file);
                if (!dataset_raw){
                    printf("Failed to parse dataset in %lldms. Common reasons are: dataset contains invalid json.\n", timer_end(dataset_parse_timer));
                    exit(1);
                }
                free(dataset_file);
                printf("Parsed dataset in %lldms.\n", timer_end(dataset_parse_timer));

                long long dataset_preprocess_timer = timer();
                printf("Preprocessing dataset...\n");
                int** dataset_token_sequences = NULL;
                size_t* dataset_token_sequences_lens = NULL;
                size_t dataset_token_sequences_len = 0;
                bool** token_masks = NULL;
                size_t* token_masks_lens = NULL;

                if (!cJSON_IsArray(dataset_raw)){
                dataset_structure_fail:
                    printf("Dataset structure is invalid. (failed dataset preprocess in %lldms)\n", timer_end(dataset_preprocess_timer));
                    exit(1);
                }
                
                dataset_token_sequences_len = cJSON_GetArraySize(dataset_raw);
                dataset_token_sequences = calloc(dataset_token_sequences_len * sizeof(int*), 1);
                token_masks = calloc(dataset_token_sequences_len * sizeof(bool*), 1);

                if ((!dataset_token_sequences) || (!token_masks)){
                dataset_failed_alloc:
                    printf("Failed to allocate memory to preprocess dataset in %lldms.\n", timer_end(dataset_preprocess_timer));
                    exit(1);
                }

                dataset_token_sequences_lens = calloc(dataset_token_sequences_len * sizeof(size_t), 1);
                token_masks_lens = calloc(dataset_token_sequences_len * sizeof(size_t), 1);
                if ((!dataset_token_sequences_lens) || (!token_masks_lens)){
                    goto dataset_failed_alloc;
                }

                cJSON* item_raw = dataset_raw->child;
                for (int index = 0; index < dataset_token_sequences_len; index++){
                    if (!cJSON_IsObject(item_raw)){
                        goto dataset_structure_fail;
                    }

                    cJSON* item_system_prompt_raw = cJSON_GetObjectItem(item_raw, "system_prompt");
                    if (!item_system_prompt_raw){
                        goto dataset_structure_fail;
                    }
                    if (!cJSON_IsString(item_system_prompt_raw)){
                        goto dataset_structure_fail;
                    }
                    
                    dataset_token_sequences[index] = NULL;
                    dataset_token_sequences_lens[index] = 0;
                    token_masks[index] = NULL;
                    token_masks_lens[index] = 0;

                    char* system_segment = malloc(strlen("<|bos|>\nSystem: ") + strlen(item_system_prompt_raw->valuestring) + 1);
                    if (!system_segment){
                        goto dataset_failed_alloc;
                    }
                    strcpy(system_segment, "<|bos|>\nSystem: ");
                    strcat(system_segment, item_system_prompt_raw->valuestring);

                    tokenize_ret system_tokens = tokenize(system_segment, false);
                    free(system_segment);

                    dataset_token_sequences[index] = malloc(system_tokens.seq_len * sizeof(int));
                    if (!dataset_token_sequences[index]){
                        free_tokenize_ret(system_tokens);
                        goto dataset_failed_alloc;
                    }
                    memcpy(dataset_token_sequences[index], system_tokens.tokens, system_tokens.seq_len * sizeof(int));
                    dataset_token_sequences_lens[index] = system_tokens.seq_len;

                    token_masks[index] = malloc(system_tokens.seq_len * sizeof(bool));
                    if (!token_masks[index]){
                        free_tokenize_ret(system_tokens);
                        goto dataset_failed_alloc;
                    }
                    memset(token_masks[index], false, system_tokens.seq_len);
                    token_masks_lens[index] = system_tokens.seq_len;

                    free_tokenize_ret(system_tokens);

                    cJSON* turns_raw = cJSON_GetObjectItem(item_raw, "turns");
                    if (!turns_raw){
                        goto dataset_structure_fail;
                    }
                    if (!cJSON_IsArray(turns_raw)){
                        goto dataset_structure_fail;
                    }

                    size_t turns_len = cJSON_GetArraySize(turns_raw);
                    cJSON* turn_raw = turns_raw->child;
                    for (int subindex = 0; subindex < turns_len; subindex++){
                        if (!cJSON_IsObject(turn_raw)){
                            goto dataset_structure_fail;
                        }

                        cJSON* turn_person = cJSON_GetObjectItem(turn_raw, "person");
                        cJSON* turn_model = cJSON_GetObjectItem(turn_raw, "model");
                        if ((!turn_person) || (!turn_model)){
                            goto dataset_structure_fail;
                        }
                        if ((!cJSON_IsString(turn_person)) || (!cJSON_IsString(turn_model))){
                            goto dataset_structure_fail;
                        }

                        char* context_segment = malloc(strlen("\n\nPerson:\n") + strlen(turn_person->valuestring) + strlen("\nYou:\n") + 1);
                        if (!context_segment){
                            goto dataset_failed_alloc;
                        }
                        strcpy(context_segment, "\n\nPerson:\n");
                        strcat(context_segment, turn_person->valuestring);
                        strcat(context_segment, "\nYou:\n");

                        tokenize_ret context_tokens = tokenize(context_segment, false);
                        free(context_segment);

                        size_t old_len = dataset_token_sequences_lens[index];
                        dataset_token_sequences[index] = realloc(dataset_token_sequences[index], (old_len + context_tokens.seq_len) * sizeof(int));
                        if (!dataset_token_sequences[index]){
                            free_tokenize_ret(context_tokens);
                            goto dataset_failed_alloc;
                        }
                        memcpy(dataset_token_sequences[index] + old_len, context_tokens.tokens, context_tokens.seq_len * sizeof(int));

                        size_t old_mask_len = token_masks_lens[index];
                        token_masks[index] = realloc(token_masks[index], (old_mask_len + context_tokens.seq_len) * sizeof(bool));
                        if (!token_masks[index]){
                            free_tokenize_ret(context_tokens);
                            goto dataset_failed_alloc;
                        }
                        memset(token_masks[index] + old_mask_len, false, context_tokens.seq_len);

                        dataset_token_sequences_lens[index] = old_len + context_tokens.seq_len;
                        token_masks_lens[index] = old_mask_len + context_tokens.seq_len;
                        free_tokenize_ret(context_tokens);

                        tokenize_ret response_tokens = tokenize(turn_model->valuestring, false);

                        old_len = dataset_token_sequences_lens[index];
                        dataset_token_sequences[index] = realloc(dataset_token_sequences[index], (old_len + response_tokens.seq_len) * sizeof(int));
                        if (!dataset_token_sequences[index]){
                            free_tokenize_ret(response_tokens);
                            goto dataset_failed_alloc;
                        }
                        memcpy(dataset_token_sequences[index] + old_len, response_tokens.tokens, response_tokens.seq_len * sizeof(int));

                        old_mask_len = token_masks_lens[index];
                        token_masks[index] = realloc(token_masks[index], (old_mask_len + response_tokens.seq_len) * sizeof(bool));
                        if (!token_masks[index]){
                            free_tokenize_ret(response_tokens);
                            goto dataset_failed_alloc;
                        }
                        memset(token_masks[index] + old_mask_len, true, response_tokens.seq_len);

                        dataset_token_sequences_lens[index] = old_len + response_tokens.seq_len;
                        token_masks_lens[index] = old_mask_len + response_tokens.seq_len;
                        free_tokenize_ret(response_tokens);

                        int eos_token_id = token_to_id("<|eos|>");
                        old_len = dataset_token_sequences_lens[index];
                        dataset_token_sequences[index] = realloc(dataset_token_sequences[index], (old_len + 1) * sizeof(int));
                        if (!dataset_token_sequences[index]){
                            goto dataset_failed_alloc;
                        }
                        dataset_token_sequences[index][old_len] = eos_token_id;
                        dataset_token_sequences_lens[index] = old_len + 1;

                        old_mask_len = token_masks_lens[index];
                        token_masks[index] = realloc(token_masks[index], (old_mask_len + 1) * sizeof(bool));
                        if (!token_masks[index]){
                            goto dataset_failed_alloc;
                        }
                        token_masks[index][old_mask_len] = true;
                        token_masks_lens[index] = old_mask_len + 1;

                        turn_raw = turn_raw->next;
                    }
                    
                    if (debug){
                        printf("=== DEBUG: Entry %d ===\n", index);
                        for (int dbg = 0; dbg < dataset_token_sequences_lens[index]; dbg++){
                            char* tok_str = id_to_token(dataset_token_sequences[index][dbg]);
                            printf("[%d|%s|%c] ", dataset_token_sequences[index][dbg], tok_str ? tok_str : "NULL", token_masks[index][dbg] ? 'T' : 'F');
                        }
                        printf("\n=== END DEBUG ===\n");
                    }

                    item_raw = item_raw->next;
                }

                cJSON_Delete(dataset_raw);

                printf("Preprocessed dataset in %lldms.\n", timer_end(dataset_preprocess_timer));
                
                long long dataset_count_tokens_timer = timer();
                printf("Counting tokens in dataset...\n");
                size_t total_tokens = 0;
                size_t total_entries = dataset_token_sequences_len;
                for (int index = 0; index < total_entries; index++){
                    total_tokens += dataset_token_sequences_lens[index];
                }

                double avr_tokens_per_entry = (total_entries > 0) ? (double)(total_tokens) / (double)(total_entries) : 0.0f;
                printf("%zu tokens in dataset, %.2f tok/dataset entry avr. (counted in %lldms)\n", total_tokens, avr_tokens_per_entry, timer_end(dataset_count_tokens_timer));

                long long batch_time_sums = 0;
                size_t batch_time_n = 0;
                int every_n_batch = 0;

                for (int index = 0; index < total_entries; index++){
                    long long entry_timer = timer();
                    printf("Training on entry %d/%d...\n", index + 1, total_entries);
                    int* entry_tokens = dataset_token_sequences[index];
                    size_t entry_tokens_len = dataset_token_sequences_lens[index];
                    bool* entry_mask = token_masks[index];
                    size_t entry_mask_len = token_masks_lens[index];
                    if (entry_mask_len != entry_tokens_len){
                        printf("[Dataset] Token count mismatch on entry %d: mask_len=%zu, token_len=%zu. Aborting to avoid OOB.\n", index, entry_mask_len, entry_tokens_len);
                        exit(1);
                    }

                    // Build up context as token array instead of string
                    int* context_tokens = NULL;
                    size_t context_tokens_len = 0;

                    for (int pos = 0; pos < entry_mask_len; pos++){
                        int tok_id = entry_tokens[pos];
                        if (tok_id < 0 || tok_id >= vocab_len){
                            printf("HEAP CORRUPTION DETECTED: token_id=%d out of range [0, %d)\n", tok_id, vocab_len);
                            exit(1);
                        }

                        if (!entry_mask[pos]){
                            // Non-trainable token - just add to context
                            context_tokens_len++;
                            context_tokens = realloc(context_tokens, context_tokens_len * sizeof(int));
                            if (!context_tokens){
                                printf("Failed to allocate memory to process training context tokens.\n");
                                exit(1);
                            }
                            context_tokens[context_tokens_len - 1] = tok_id;
                        }
                        else{
                            // Trainable token - create task with current context, then add token to context
                            train_step_token target = {0};
                            target.token = id_to_token(tok_id);
                            target.token_id = tok_id;

                            // Duplicate the current context tokens for the worker
                            int* context_copy = malloc(context_tokens_len * sizeof(int));
                            if (!context_copy){
                                printf("Failed to allocate memory to copy context tokens.\n");
                                exit(1);
                            }
                            memcpy(context_copy, context_tokens, context_tokens_len * sizeof(int));
                            add_to_worker_tasklist(context_copy, context_tokens_len, target);

                            // Add this token to context for next iteration
                            context_tokens_len++;
                            context_tokens = realloc(context_tokens, context_tokens_len * sizeof(int));
                            if (!context_tokens){
                                printf("Failed to allocate memory to process training context tokens.\n");
                                exit(1);
                            }
                            context_tokens[context_tokens_len - 1] = tok_id;
                        }

                        int debug_tok = dataset_token_sequences[0][0];
                        if (debug_tok < 0 || debug_tok >= vocab_len){
                            printf("CORRUPTION BEFORE flush: token[0]=%d\n", debug_tok);
                            exit(1);
                        }

                        long long timer_flush = timer();
                        float loss_sum_add = flush_worker_tasklist_if_required();
                        timer_flush = timer_end(timer_flush);

                        debug_tok = dataset_token_sequences[0][0];
                        if (debug_tok < 0 || debug_tok >= vocab_len){
                            printf("CORRUPTION AFTER flush: token[0]=%d\n", debug_tok);
                            exit(1);
                        }

                        if (loss_sum_add != -1){
                            batch_time_sums += timer_flush;
                            batch_time_n++;
                            loss_sum += loss_sum_add;
                            loss_n++;
                            if (batch_time_n > 0){
                                size_t remaining_tokens = 0;
                                for (int future_entry = index + 1; future_entry < total_entries; future_entry++){
                                    remaining_tokens += dataset_token_sequences_lens[future_entry];
                                }
                                remaining_tokens += entry_mask_len - pos;
                                
                                long long eta_ms = batch_time_sums / batch_time_n * (int)(remaining_tokens / batchSize);
                                int hours = eta_ms / 3600000;
                                int mins = (eta_ms % 3600000) / 60000;
                                int secs = (eta_ms % 60000) / 1000;
                                printf("\n------------------\n");
                                
                                if (eta_pause_toggle && every_n_batch == eta_pause_every_n_batch - 1){
                                    printf("ETA for current dataset: %02dh%02dm%02ds\n", hours, mins, secs);
                                    printf("(Pausing for %dms for you to see ETA)\n\n", eta_pause_time_ms);
                                    sleep_ms(eta_pause_time_ms);
                                    every_n_batch = 0;
                                }
                                else{
                                    printf("ETA for current dataset: %02dh%02dm%02ds\n\n", hours, mins, secs);
                                    every_n_batch++;
                                }
                            }
                            else{
                                printf("\n------------------\n");
                                printf("ETA for current dataset: Not enough data to estimate.\n\n");
                            }
                        }
                    }
                    free(context_tokens);
                    printf("Trained on entry %d/%d in %lldms.\n", index + 1, total_entries, timer_end(entry_timer));
                }
                printf("Flushing remaining worker tasks (if any)...\n");
                long long timer_remaining = timer();
                float loss_avr_remaining = flush_worker_tasklist(); //If there are any additional things left for some reason
                if (!loss_avr_remaining == -1){
                    printf("There weren't any remaining worker tasks to flush (found out in %lldms).\n", timer_end(timer_remaining));
                }
                else{
                    printf("Flushed remaining worker tasks with avr loss %f in %lldms.\n", loss_avr_remaining, timer_end(timer_remaining));
                }

                for (int index = 0; index < total_entries; index++){
                    free(dataset_token_sequences[index]);
                    free(token_masks[index]);
                }
                free(dataset_token_sequences);
                free(dataset_token_sequences_lens);
                free(token_masks);
                free(token_masks_lens);
                printf("Trained using dataset %d/%d in %lldms.\n", dataset + 1, training_dataset_paths_len, timer_end(dataset_timer));
            }
            float loss_avr = loss_sum / (float)(loss_n);
            loss_history_len++;
            loss_history = realloc(loss_history, loss_history_len * sizeof(float));
            if (!loss_history){
                printf("Failed to allocate memory to track loss history.\n");
                exit(1);
            }
            loss_history[loss_history_len - 1] = loss_avr;
            printf("Finished epoch %d/%d in %lldms.\n", epoch + 1, epochAmount, timer_end(epoch_timer));

            printf("Autosave is %s.\n", autosave ? "on" : "off");
            float current_loss = loss_avr;
            if (autosave){
                char filename[256];
                sprintf(filename, "model_%.6f.zip", current_loss);

                int modifier = 1;
                while (file_exists(filename)){
                    sprintf(filename, "model_%.6f_%d.zip", current_loss, modifier);
                    modifier++;
                }

                save(filename);
            }

            char* checkpoint_response = input_with_timeout("Do you want to open checkpoint cli? Write anything and press enter if so (30 seconds to answer): ", 30000);
            if (checkpoint_response){
                free(checkpoint_response);
                bool should_stop = training_interactive_cli(loss_avr, loss_history, loss_history_len, epoch + 1, epochAmount);
                if (should_stop){
                    break;
                }
            }
        }
        printf("Finished training for %d epochs in %lldms.\n", epochAmount, timer_end(train_timer));
        return;
    }
    
    if (do_pretrain){
        pretrain(pre_train_epochs);
    }
    if (do_train) {
        train(train_epochs);
    }

    printf("\n-----------------------------------------------------\n");
        printf("Chat with the model (inference, default temp is 0):\n");
        float temp = 0;
    printf("Commands:\n");
    printf("  /exit                      -- Exit the cli.\n");
    printf("  /save <filename>           -- Save model.\n");
    printf("  /temperature [temperature] -- Display/set current temperature.\n");
    printf("  /reset_context             -- Reset conversation context.\n");
    printf("\n");
    printf("Do you want to enable multiline inputs? This will allow you to send messages multiple lines long, however as enter will be used to create a new line, you will have to use ctrl + d twice to send your message/command instead of just pressing enter.\n");
    char* multilineornot = NULL;
    bool use_multiline = false;
    while (true){
        multilineornot = input("Enable multiline inputs? (y/n) ");
        if (!multilineornot){
            printf("Failed to read user input.\n");
            return 1;
        }

        if (strcmp(multilineornot, "y") == 0){
            printf("Enabled multiline inputs.\n");
            printf("\n");
            free(multilineornot);
            multilineornot = NULL;
            use_multiline = true;
            break;
        }
        else{
            if (strcmp(multilineornot, "n") == 0){
                printf("Disabled multiline inputs.\n");
                printf("\n");
                free(multilineornot);
                multilineornot = NULL;
                use_multiline = false;
                break;
            }
            else{
                printf("Invalid input, type 'y' for yes or 'n' for no.\n");
                free(multilineornot);
                multilineornot = NULL;
                continue;
            }
        }
    }

    // Precompute template tokens
    tokenize_ret bos_system_tok = tokenize("<|bos|>\nSystem: ", false);
    if (!bos_system_tok.success){
        printf("Failed to tokenize bos_system template.\n");
        return 1;
    }
    tokenize_ret person_tok = tokenize("\n\nPerson:\n", false);
    if (!person_tok.success){
        printf("Failed to tokenize person template.\n");
        free_tokenize_ret(bos_system_tok);
        return 1;
    }
    tokenize_ret you_tok = tokenize("\nYou:\n", false);
    if (!you_tok.success){
        printf("Failed to tokenize you template.\n");
        free_tokenize_ret(bos_system_tok);
        free_tokenize_ret(person_tok);
        return 1;
    }

    // Initialize context with bos_system tokens
    int* context_tokens = malloc(bos_system_tok.seq_len * sizeof(int));
    if (!context_tokens){
        printf("Failed to allocate memory to setup model chat interface.\n");
        free_tokenize_ret(bos_system_tok);
        free_tokenize_ret(person_tok);
        free_tokenize_ret(you_tok);
        return 1;
    }
    memcpy(context_tokens, bos_system_tok.tokens, bos_system_tok.seq_len * sizeof(int));
    size_t context_tokens_len = bos_system_tok.seq_len;

    while (true){
        printf("\nYou:\n");
        char* input_to_inference = use_multiline ? input_multiline("› ") : input("› ");
        if (!input_to_inference){
            printf("Failed to read user input.\n");
            return 1;
        }

        size_t input_to_inference_len = strlen(input_to_inference);
        if (strcmp(input_to_inference, "/exit") == 0){
            return 0;
        }
        else if (strcmp(input_to_inference, "/reset_context") == 0){
            free(input_to_inference);
            free(context_tokens);
            context_tokens = malloc(bos_system_tok.seq_len * sizeof(int));
            if (!context_tokens){
                printf("Failed to allocate memory to reset context.\n");
                free_tokenize_ret(bos_system_tok);
                free_tokenize_ret(person_tok);
                free_tokenize_ret(you_tok);
                return 1;
            }
            memcpy(context_tokens, bos_system_tok.tokens, bos_system_tok.seq_len * sizeof(int));
            context_tokens_len = bos_system_tok.seq_len;
            printf("Context has been reset.\n");
            continue;
        }
        else{
            if (input_to_inference_len >= strlen("/save x")){
                char charaftersave = input_to_inference[strlen("/save ")];
                input_to_inference[strlen("/save ")] = '\0';
                if (strcmp(input_to_inference, "/save ") == 0){
                    input_to_inference[strlen("/save ")] = charaftersave;
                    char* newptr = input_to_inference + strlen("/save ");
                    save(newptr);
                    printf("Saved model as '%s'\n", newptr);
                    free(input_to_inference);
                    continue;
                }
                else{
                    if (input_to_inference_len >= strlen("/temperature x")){
                        input_to_inference[strlen("/save ")] = charaftersave;
                        charaftersave = input_to_inference[strlen("/temperature ")];
                        input_to_inference[strlen("/temperature ")] = '\0';
                        if (strcmp(input_to_inference, "/temperature ") == 0){
                            input_to_inference[strlen("/temperature ")] = charaftersave;
                            char* newptr = input_to_inference + strlen("/temperature ");
                            float ntemp = (float)(atof(newptr));
                            if (ntemp < 0){
                                printf("Invalid temperature, must be float >=0.\n");
                                free(input_to_inference);
                                continue;
                            }
                            temp = ntemp;
                            printf("Temperature now set to %f.\n", temp);
                            free(input_to_inference);
                            continue;
                        }
                        else{
                            input_to_inference[strlen("/temperature ")] = charaftersave;
                            goto generate_turn;
                        }
                    }
                    else{
                        input_to_inference[strlen("/save ")] = charaftersave;
                        if (strcmp(input_to_inference, "/temperature") == 0){
                            printf("Current temperature: %.2f\n", temp);
                            printf("\n");
                            printf("You can also specify a new temperature like this:\n");
                            printf("  /temperature <new_temperature>\n");
                            free(input_to_inference);
                            continue;
                        }
                        else{
                            goto generate_turn;
                        }
                    }
                }
            }
            else{
                if (strcmp(input_to_inference, "/save") == 0){
                    printf("You need to provide a filename with /save.\n");
                    printf("Usage: /save <filename>\n");
                    printf("(With <filename> being any valid filename)\n");
                    free(input_to_inference);
                    continue;
                }
                else{
                    goto generate_turn;
                }
            }
        }

    generate_turn:
        // Tokenize user input
        tokenize_ret input_tok = tokenize(input_to_inference, false);
        if (!input_tok.success){
            printf("Failed to tokenize user input.\n");
            free(input_to_inference);
            continue;
        }

        // Append person_tok + input_tok + you_tok to context
        size_t new_len = context_tokens_len + person_tok.seq_len + input_tok.seq_len + you_tok.seq_len;
        context_tokens = realloc(context_tokens, new_len * sizeof(int));
        if (!context_tokens){
            printf("Failed to allocate memory to track chat context.\n");
            free_tokenize_ret(input_tok);
            return 1;
        }
        memcpy(context_tokens + context_tokens_len, person_tok.tokens, person_tok.seq_len * sizeof(int));
        context_tokens_len += person_tok.seq_len;
        memcpy(context_tokens + context_tokens_len, input_tok.tokens, input_tok.seq_len * sizeof(int));
        context_tokens_len += input_tok.seq_len;
        memcpy(context_tokens + context_tokens_len, you_tok.tokens, you_tok.seq_len * sizeof(int));
        context_tokens_len += you_tok.seq_len;
        free_tokenize_ret(input_tok);

        printf("Model:\n");
        long long generate_timer = timer();
        size_t token_n = 0;
        while (true){
            if (token_n + 1 > maxOutputSize){
                printf("[Reached max output size]\n");
                break;
            }
            if (context_tokens_len >= contextSize){
                printf("[Context full]\n");
                break;
            }
            infret rets = inference(context_tokens, context_tokens_len, false, temp, false);
            if (!rets.success){
                printf("[Inference failure]\n");
                break;
            }
            token_n++;
            int predicted_token_id = rets.predicted_token_id;
            char* predicted_token = id_to_token(predicted_token_id);
            bool eos = false;
            if (strcmp(predicted_token, "<|eos|>") == 0){
                eos = true;
                goto after_inference_print;
            }

        after_inference_print:
            printf("%s", predicted_token);
            fflush(stdout);

            // Append predicted token ID to context
            context_tokens = realloc(context_tokens, (context_tokens_len + 1) * sizeof(int));
            if (!context_tokens){
                printf("Failed to allocate memory to track chat context.\n");
                return 1;
            }
            context_tokens[context_tokens_len] = predicted_token_id;
            context_tokens_len++;

            if (eos){
                break;
            }
        }
        long long timerresult = timer_end(generate_timer);
        free(input_to_inference);
        float tok_per_s = 0;
        if (timerresult > 0){
            tok_per_s = 1000 * (float)(token_n) / (float)(timerresult);
        }
        printf("(Generated in %lldms, %d tokens, %.2f tok/sec avr.)\n", timerresult, (int)token_n, tok_per_s);
    }

    //NOTE: Old demos
    //
    // char* saveornot = NULL;
    // while (true){
    //     saveornot = input("Do you want to save the current model? (y/n) ");
    //     if (strcmp(saveornot, "y") == 0){
    //         save("bruh.zip");
    //         free(saveornot);
    //         break;
    //     }
    //     else{
    //         if (strcmp(saveornot, "n") == 0){
    //             printf("alr\n");
    //             free(saveornot);
    //             break;
    //         }
    //         else{
    //             printf("Enter y for yes or n for no.\n");
    //             free(saveornot);
    //         }
    //     }
    // }
    //
    // printf("Enter strings to tokenize:\n");
    // while (true){
    //     char* in = input("› ");
    //     int* tokens = tokenize(in);
    //     free(in);
    //
    //     printf("Token ids: ");
    //     for (int index = 1; index < tokens[0] + 1; index++){
    //         printf("%d ", tokens[index]);
    //     }
    //     printf("\n");
    //     printf("Tokens: ");
    //     for (int index = 1; index < tokens[0] + 1; index++){
    //         printf("\"%s\" ", id_to_token(tokens[index]));
    //     }
    //     printf("\n");
    // }
    //

    return 0;
}
