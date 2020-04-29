// ======================================================================== //
// ====================            SHELL            ======================= //
//      2019 Dec                                       Panfilov 211 group   //

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/file.h>

#define B_BUF 8
#define S_BUF 4

// Name arg1 arg2 arg3 ...
struct SimpleCommand {
    char* Name;
    char** Arg; // Args[0] stores a copy of Name
};

// <file SimpleCmd | SimpleCmd | ...
struct Command {
    struct SimpleCommand** SimpleCmd; // Conveyer
    struct ConditionalCommand* Bracket; // Brackets
    int IfSuccess; // if && after Command
    int IfUnsuccess; // if || after Command
    char* Input;
    char* Output;
    char* OutputSave;
};

// Cmd && Cmd || Cmd && Cmd && (Cmd || Cmd) ...
struct ConditionalCommand {
    struct Command** Cmd;
    int BackGr; // if & after ConditionalCommand
};

// CondCmd & CondCmd ; CondCmd & CondCmd ; CondCmd ; CondCmd & ...
struct ShellCommand {
    struct ConditionalCommand** CondCmd;
};



char* get_inpstr(void) { // Ввод строки с сжиманием подряд идущих пробелов в один
    char* InpS;
    char c;
    int i;
    int InpStrBuf = B_BUF;

    InpS = malloc(B_BUF);
    for (i = 0; (c = getchar()) != '\n'; i++) {
        if (c == ' ') {
            InpS[i] = c;
            i++;
            while ((c = getchar()) == ' ');
        }
        if (c == '\n') {
            InpS[i-1] = '\0';
            break;
        }
        if (i >= InpStrBuf - 1) {
            InpStrBuf += B_BUF;
            InpS = realloc(InpS, InpStrBuf);
        }
        InpS[i] = c;
    }
    InpS[i] = '\0';

    return InpS;
}

int execute_condcmd(struct ConditionalCommand* CondCmd); // Прототип

int execute_cmd(struct Command* Cmd) {
    pid_t pid1, pid2;
    int CSimpleCmd = 0, status, status2;
    int fd[2], file;

    if (Cmd->Bracket != NULL) { // Скобки
        if (execute_condcmd(Cmd->Bracket) == -1) {
            return -1;
        }
        else{
            return 0;
        }
    }

    if ((pid1 = fork()) < 0) {
        printf("error: fork 1 doesn't work. \n");
        return -1;
    }

    if (!pid1) { // Сын
        if (Cmd->Input != NULL) {
            printf("input file is opening... \n");
            if ((file = open(Cmd->Input, O_RDONLY)) == -1) {
                perror("error");
                exit(-1);
            }
            dup2(file, 0);
            close(file);
        }
        if (Cmd->Output != NULL) {
            printf("output file is opening... \n");
            if ((file = open(Cmd->Output, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
                perror("error");
                exit(-1);
            }
            dup2(file, 1);
            close(file);
        }
        if (Cmd->OutputSave != NULL) {
            printf("output file is opening... \n");
            if ((file = open(Cmd->OutputSave, O_WRONLY | O_CREAT | O_APPEND, 0666)) == -1) {
                perror("error");
                exit(-1);
            }
            dup2(file, 1);
            close(file);
        }

        pipe(fd);

        while (Cmd->SimpleCmd[CSimpleCmd] != NULL) {
            if ((pid2 = fork()) < 0) {
                printf("error: fork 2 doesn't work. \n");
                return -1;
            }

            if (!pid2) { // Сын

                if (CSimpleCmd > 0) {
                    dup2(fd[0], 0);
                    close(fd[0]);
                    close(fd[1]);
                }
                if (Cmd->SimpleCmd[CSimpleCmd + 1] != NULL) {
                    dup2(fd[1], 1);
                    close(fd[1]);
                    close(fd[0]);
                }

                execvp(Cmd->SimpleCmd[CSimpleCmd]->Name, Cmd->SimpleCmd[CSimpleCmd]->Arg);
                perror("error");
                exit(-1);
            }

            CSimpleCmd++;
        }

        for (int i = 1; i <= CSimpleCmd; i++) {
            waitpid(0, &status2, 0);
            close(fd[1]);
            if (WEXITSTATUS(status2) != 0) {
                exit(-1);
            }
        }
        exit(0);
    }
    else { // Отец
        waitpid(pid1, &status, 0);
        if (WEXITSTATUS(status) != 0) {
            return -1;
        }
        else {
            return 0;
        }
    }
}

int execute_condcmd(struct ConditionalCommand* CondCmd) {
    int CCmd = 0, file;
    pid_t pid;
    int UnsuccessFlag = 0, SuccessFlag = 0;

    if (CondCmd->BackGr) { // Фоновый режим
        if(!(pid = fork())) {
            if(!(pid = fork())) {
                kill(getppid(), SIGKILL);
                printf("\n[ ] background %d started. \n", getpid());

                signal(SIGINT, SIG_IGN);

                file = open("/dev/null", O_RDONLY);
                dup2(file, 0);
                close(file);

                /*file = open("/dev/null", O_WRONLY, 0666);
                dup2(file, 1);
                close(file);*/

                while (CondCmd->Cmd[CCmd] != NULL) { // Нефоновый режим

                    if (CondCmd->Cmd[CCmd]->IfSuccess == 1) {


                        //printf("Here is Success ... \n");
                        if (SuccessFlag) {
                            SuccessFlag = 0;
                            CCmd++;
                            continue;
                        }
                        if (UnsuccessFlag) {
                            CCmd++;
                            continue;
                        }
                        if (execute_cmd(CondCmd->Cmd[CCmd]) == -1) {
                            UnsuccessFlag = 1;
                            CCmd++;
                            continue;
                        }
                        CCmd++;
                        continue;


                    }
                    if (CondCmd->Cmd[CCmd]->IfUnsuccess == 1) {


                        //printf("Here is Unuccess ... \n");
                        if (UnsuccessFlag) {
                            UnsuccessFlag = 0;
                            CCmd++;
                            continue;
                        }
                        if (SuccessFlag) {
                            CCmd++;
                            continue;
                        }
                        if (execute_cmd(CondCmd->Cmd[CCmd]) == 0) {
                            SuccessFlag = 1;
                            CCmd++;
                            continue;
                        }
                        CCmd++;
                        continue;


                    }
                    if (CondCmd->Cmd[CCmd]->IfSuccess == 0 && CondCmd->Cmd[CCmd]->IfUnsuccess == 0) {
                        //printf("Here is The last ... \n");
                        if (UnsuccessFlag) {
                            printf("[ ] background %d finished with failure. \n", getpid());
                            exit(-1);
                        }
                        if (SuccessFlag) {
                            printf("[ ] background %d finished with failure. \n", getpid());
                            exit(-1);
                        }
                        if (execute_cmd(CondCmd->Cmd[CCmd]) == -1) {
                            printf("[ ] background %d finished with failure. \n", getpid());
                            exit(-1);
                        }
                        printf("[ ] background %d finished. \n", getpid());
                        exit(0);
                    }
                }

                /*while (CondCmd->Cmd[CCmd] != NULL) {
                    if (CondCmd->Cmd[CCmd]->IfSuccess == 1) {
                        if (execute_cmd(CondCmd->Cmd[CCmd]) == -1) break;
                    }
                    if (CondCmd->Cmd[CCmd]->IfUnsuccess == 1) {
                        if (execute_cmd(CondCmd->Cmd[CCmd]) == 0) break;
                    }
                    if (CondCmd->Cmd[CCmd]->IfSuccess == 0 && CondCmd->Cmd[CCmd]->IfUnsuccess == 0) {
                        execute_cmd(CondCmd->Cmd[CCmd]);
                    }
                    CCmd++;
                }*/

                printf("[ ] background %d finished. \n", getpid());
                exit(0);
            }
            else {
                while(1);
            }
        }
        waitpid(pid, NULL, 0);

    }
    else {
        while (CondCmd->Cmd[CCmd] != NULL) { // Нефоновый режим

            if (CondCmd->Cmd[CCmd]->IfSuccess == 1) {


                //printf("Here is Success ... \n");
                if (SuccessFlag) {
                    SuccessFlag = 0;
                    CCmd++;
                    continue;
                }
                if (UnsuccessFlag) {
                    CCmd++;
                    continue;
                }
                if (execute_cmd(CondCmd->Cmd[CCmd]) == -1) {
                    UnsuccessFlag = 1;
                    CCmd++;
                    continue;
                }
                CCmd++;
                continue;


            }
            if (CondCmd->Cmd[CCmd]->IfUnsuccess == 1) {


                //printf("Here is Unuccess ... \n");
                if (UnsuccessFlag) {
                    UnsuccessFlag = 0;
                    CCmd++;
                    continue;
                }
                if (SuccessFlag) {
                    CCmd++;
                    continue;
                }
                if (execute_cmd(CondCmd->Cmd[CCmd]) == 0) {
                    SuccessFlag = 1;
                    CCmd++;
                    continue;
                }
                CCmd++;
                continue;


            }
            if (CondCmd->Cmd[CCmd]->IfSuccess == 0 && CondCmd->Cmd[CCmd]->IfUnsuccess == 0) {
                //printf("Here is The last ... \n");
                if (UnsuccessFlag) {
                    return -1;
                }
                if (SuccessFlag) {
                    return -1;
                }
                if (execute_cmd(CondCmd->Cmd[CCmd]) == -1) {
                    return -1;
                }
                return 0;
            }

            return 0;
        }
        return 0;
    }
}

void execute_shellcmd(struct ShellCommand* ShellCmd) {
    int CCondCmd = 0;

    while (ShellCmd->CondCmd[CCondCmd] != NULL) {
        execute_condcmd(ShellCmd->CondCmd[CCondCmd]);

        CCondCmd++;
    }
}

void fill_simple_cmd (struct SimpleCommand* SimpleCmd, char* InpStr) {
    int CountArg = 0;
    int InpStrPoint = 0;
    int ArgStrPoint = 0;
    int ArrayBuf = sizeof(char*);
    int ArgStrBuf = S_BUF;
    char* Arg = NULL;
    char c;

    SimpleCmd->Arg = malloc(ArrayBuf);
    SimpleCmd->Arg[0] = NULL;

    while ((c = InpStr[InpStrPoint]) != '\0') {
        if (SimpleCmd->Arg[CountArg] == NULL) {
            SimpleCmd->Arg[CountArg] = "p";

            ArrayBuf += sizeof(char*);
            SimpleCmd->Arg = realloc(SimpleCmd->Arg, ArrayBuf);
            SimpleCmd->Arg[CountArg + 1] = NULL;
            Arg = malloc(S_BUF);
            ArgStrBuf = S_BUF;
        }
        if (c == ' ') {
            Arg[ArgStrPoint] = '\0';
            if (!CountArg) {
                SimpleCmd->Name = malloc(ArgStrBuf);

                strcpy(SimpleCmd->Name, Arg);
                //printf("-   -   -   Name '%s' is created. \n", Arg);
            }
            SimpleCmd->Arg[CountArg] = malloc(ArgStrBuf);

            strcpy(SimpleCmd->Arg[CountArg], Arg);
            //printf("-   -   -   Arg '%s' is created. \n", Arg);

            free(Arg);
            CountArg++;
            InpStrPoint++;
            ArgStrPoint = 0;
            continue;
        }
        if (ArgStrPoint >= ArgStrBuf - 1) {
            ArgStrBuf += S_BUF;
            Arg = realloc(Arg, ArgStrBuf);
        }
        if (c == ' ' && !ArgStrPoint) {
            InpStrPoint++;
            c = InpStr[InpStrPoint];
        }
        Arg[ArgStrPoint] = c;
        InpStrPoint++;
        ArgStrPoint++;
    }
    if (InpStr[InpStrPoint - 1] != ' ') {
        Arg[ArgStrPoint] = '\0';
        if (!CountArg) {
            SimpleCmd->Name = malloc(ArgStrBuf);

            strcpy(SimpleCmd->Name, Arg);
            //printf("-   -   -   Name '%s' is created. \n", SimpleCmd->Name);
        }
        SimpleCmd->Arg[CountArg] = malloc(ArgStrBuf);

        strcpy(SimpleCmd->Arg[CountArg], Arg);
        //printf("-   -   -   Arg '%s' is created. \n", Arg);

        free(Arg);
    }
}

void fill_cond_cmd (struct ConditionalCommand* CondCmd, char* InpStr, int BG); // Прототип

void fill_cmd (struct Command* Cmd, char* InpStr, int IfS, int IfUS, int BracketFlag) {
    int CountSimpleCmd = 0;
    int InpStrPoint = 0;
    int CmdStrPoint = 0;
    int ArrayBuf = sizeof(struct SimpleCommand*);
    int CmdStrBuf = B_BUF;
    int InpOutFlag = 0, InpOutType;
    char* InpOutStr;
    char* SimpleCmd = NULL;
    char c;

    Cmd->SimpleCmd = malloc(ArrayBuf);
    Cmd->SimpleCmd[0] = NULL;
    Cmd->IfSuccess = IfS;
    Cmd->IfUnsuccess = IfUS;
    Cmd->Input = NULL;
    Cmd->Output = NULL;
    Cmd->OutputSave = NULL;
    Cmd->Bracket = NULL;

    if (BracketFlag) { // Если переданная Cmd является скобкой
        Cmd->Bracket = malloc(sizeof(struct ConditionalCommand));
        fill_cond_cmd(Cmd->Bracket, InpStr, 0);
        return;
    }

    //-------------------- Ввод файлов ввода - вывода -----------------------//
    while ((c = InpStr[InpStrPoint]) == '<' || (c = InpStr[InpStrPoint]) == '>') {
        InpStrPoint++;
        if (c == '<') {
            InpOutType = 0;
        }
        if (c == '>') {
            InpOutType = 1;
        }
        if (InpStr[InpStrPoint] == '>') {
            InpStrPoint++;
            InpOutType = 2;
        }
        if (InpStr[InpStrPoint] == ' ') {
            InpStrPoint++;
        }
        InpOutStr = malloc(B_BUF);
        while ((c = InpStr[InpStrPoint]) != ' ') {
            if (CmdStrPoint >= CmdStrBuf - 1) {
                CmdStrBuf += S_BUF;
                InpOutStr = realloc(InpOutStr, CmdStrBuf);
            }
            InpOutStr[CmdStrPoint] = c;
            InpStrPoint++;
            CmdStrPoint++;
        }
        InpOutStr[CmdStrPoint] = '\0';
        CmdStrPoint = 0;
        InpStrPoint++;
        if (InpOutType == 0) {
            Cmd->Input = InpOutStr;
            //printf("Writed input... \n");
        }
        if (InpOutType == 1) {
            Cmd->Output = InpOutStr;
            //printf("Writed output... \n");
        }
        if (InpOutType == 2) {
            Cmd->OutputSave = InpOutStr;
            //printf("Writed output save... \n");
        }
        //printf("-   -   InputOutput: '%s' \n", InpOutStr);
        InpOutFlag = 1;
        CmdStrBuf = B_BUF;
    }

    while ((c = InpStr[InpStrPoint]) != '\0') {
        if (Cmd->SimpleCmd[CountSimpleCmd] == NULL) {
            Cmd->SimpleCmd[CountSimpleCmd] = malloc(sizeof(struct SimpleCommand));

            ArrayBuf += sizeof(struct SimpleCommand*);
            Cmd->SimpleCmd = realloc(Cmd->SimpleCmd, ArrayBuf);
            Cmd->SimpleCmd[CountSimpleCmd + 1] = NULL;
            SimpleCmd = malloc(B_BUF);
            CmdStrBuf = B_BUF;
        }
        if (c == '|') {
            if (SimpleCmd[CmdStrPoint - 1] == ' ') {
                SimpleCmd[CmdStrPoint - 1] = '\0';
            }
            else SimpleCmd[CmdStrPoint] = '\0';

            //printf("-   -   SimpleCommand '%s' is created. \n", SimpleCmd);
            fill_simple_cmd(Cmd->SimpleCmd[CountSimpleCmd], SimpleCmd);

            free(SimpleCmd);
            CountSimpleCmd++;
            InpStrPoint ++;
            CmdStrPoint = 0;
            continue;
        }

        if (CmdStrPoint >= CmdStrBuf - 1) {
            CmdStrBuf += B_BUF;
            SimpleCmd = realloc(SimpleCmd, CmdStrBuf);
        }

        if (c == ' ' && !CmdStrPoint) {
            InpStrPoint++;
            c = InpStr[InpStrPoint];
        }
        SimpleCmd[CmdStrPoint] = c;
        InpStrPoint++;
        CmdStrPoint++;
    }
    //---------------------------- Частный случай конца строки ----------//
    if (InpStr[InpStrPoint - 1] != '|') {
        SimpleCmd[CmdStrPoint] = '\0';

        //printf("-   -   SimpleCommand '%s' is created. \n", SimpleCmd);
        fill_simple_cmd(Cmd->SimpleCmd[CountSimpleCmd], SimpleCmd);

        free(SimpleCmd);
    }
}

void fill_cond_cmd (struct ConditionalCommand* CondCmd, char* InpStr, int BG) {
    int CountCmd = 0;
    int InpStrPoint = 0;
    int CmdStrPoint = 0;
    int ArrayBuf = sizeof(struct Command*);
    int CmdStrBuf = B_BUF;
    int IfSuccess = 0, IfUnsuccess = 0, BracketFlag = 0;
    char* Cmd = NULL;
    char c;

    //------------------------ Выделение памяти ---------------------------//
    CondCmd->Cmd = malloc(ArrayBuf);
    CondCmd->Cmd[0] = NULL;
    CondCmd->BackGr = BG;

    while ((c = InpStr[InpStrPoint]) != '\0') {
        if (CondCmd->Cmd[CountCmd] == NULL) {
            CondCmd->Cmd[CountCmd] = malloc(sizeof(struct Command));

            ArrayBuf += sizeof(struct Command*);
            CondCmd->Cmd = realloc(CondCmd->Cmd, ArrayBuf);
            CondCmd->Cmd[CountCmd + 1] = NULL;
            Cmd = malloc(B_BUF);
        }

        if (c == ' ' && !CmdStrPoint) {
            InpStrPoint++;
            c = InpStr[InpStrPoint];
        }

        if (c == '(') { // С этого момента функция будет искать закрывающую скобку
            BracketFlag = 1;
            InpStrPoint++;
            c = InpStr[InpStrPoint];
        }

        if (BracketFlag && c == ')' ||
            (!BracketFlag && (c == '&' || c == '|' && InpStr[InpStrPoint + 1] == '|'))) {


            if (Cmd[CmdStrPoint - 1] == ' ') {
                Cmd[CmdStrPoint - 1] = '\0';
            }
            else Cmd[CmdStrPoint] = '\0';

            if (BracketFlag) {
                InpStrPoint++;
                if (InpStr[InpStrPoint] == ' ') {
                    InpStrPoint++;
                }
                c = InpStr[InpStrPoint];
                if (c == '\0') {
                    InpStrPoint -= 2;
                }
            }
            IfSuccess = (c == '&') ? 1 : 0;
            IfUnsuccess = (c == '|') ? 1 : 0;

            /*if (!BracketFlag) {
                printf("-   Command '%s' is created. \n", Cmd);
            }
            else {
                printf("-   Bracket '%s' is created. \n", Cmd);
            }*/

            fill_cmd(CondCmd->Cmd[CountCmd], Cmd, IfSuccess, IfUnsuccess, BracketFlag);

            free(Cmd);
            CountCmd++;
            BracketFlag = 0;
            InpStrPoint += 2;
            CmdStrPoint = 0;
            continue;
        }

        if (CmdStrPoint >= CmdStrBuf - 1) {
            CmdStrBuf += B_BUF;
            Cmd = realloc(Cmd, CmdStrBuf);
        }
        Cmd[CmdStrPoint] = c;
        InpStrPoint++;
        CmdStrPoint++;
    }
    // ---------------------------- Частный случай конца строки ------------------------- //
    if (InpStr[InpStrPoint - 1] != '&' && InpStr[InpStrPoint - 1] != '|' && InpStr[InpStrPoint - 1] != ')') {
        Cmd[CmdStrPoint] = '\0';
        IfSuccess = (c == '&') ? 1 : 0;
        IfUnsuccess = (c == '|') ? 1 : 0;

        //printf("-   Command '%s' is created. \n", Cmd);
        fill_cmd(CondCmd->Cmd[CountCmd], Cmd, IfSuccess, IfUnsuccess, 0);

        free(Cmd);
    }
}

struct ShellCommand* fill_struct(char* InpStr) {
    int CountCondCmd = 0;
    int InpStrPoint = 0;
    int CondStrPoint = 0;
    int ArrayBuf = sizeof(struct ConditionalCommand*);
    int CondStrBuf = B_BUF;
    int BackGr = 0;
    struct ShellCommand* ShellCmd;
    char* CondCmd;
    char c;

    // ----------------------------- Выделение памяти ------------------------- //
    ShellCmd = malloc(sizeof(struct ShellCommand));
    ShellCmd->CondCmd = malloc(ArrayBuf);
    ShellCmd->CondCmd[0] = NULL;

    // ----------------------------- Обработка строки ------------------------- //
    while ((c = InpStr[InpStrPoint]) != '\0') {
        if (ShellCmd->CondCmd[CountCondCmd] == NULL) { // Начало обработки команды
            ShellCmd->CondCmd[CountCondCmd] = malloc(sizeof(struct ConditionalCommand));

            ArrayBuf += sizeof(struct ConditionalCommand*);
            ShellCmd->CondCmd = realloc(ShellCmd->CondCmd, ArrayBuf);
            ShellCmd->CondCmd[CountCondCmd + 1] = NULL;
            CondCmd = malloc(B_BUF);
        }

        if (c == '&' && InpStr[InpStrPoint + 1] != '&' || c == ';') { // Конец обработки команды
            if (CondCmd[CondStrPoint - 1] == ' ') {
                CondCmd[CondStrPoint - 1] = '\0';
            }
            else CondCmd[CondStrPoint] = '\0';

            BackGr = (c == '&') ? 1 : 0;

            //printf("Conditional Command '%s' is created. \n", CondCmd);
            fill_cond_cmd(ShellCmd->CondCmd[CountCondCmd], CondCmd, BackGr);

            free(CondCmd);
            CountCondCmd++;
            InpStrPoint++;
            CondStrPoint = 0;
            continue;
        }

        if (CondStrPoint >= CondStrBuf - 2) { // Добавление памяти
            CondStrBuf += B_BUF;
            CondCmd = realloc(CondCmd, CondStrBuf);
        }

        if (c == '&' && InpStr[InpStrPoint + 1] == '&') { // Если встретится && , его нужно обойти
            CondCmd[CondStrPoint] = c;
            InpStrPoint++;
            CondStrPoint++;
        }

        if (c == ' ' && !CondStrPoint) { // Пропуск пробела при начале обработки очередной команды
            InpStrPoint++;
            c = InpStr[InpStrPoint];
        }
        CondCmd[CondStrPoint] = c;
        InpStrPoint++;
        CondStrPoint++;
    }
    if (InpStr[InpStrPoint - 1] != '&' && InpStr[InpStrPoint - 1] != ';') { // Частный случай конца строки
        CondCmd[CondStrPoint] = '\0';
        BackGr = (c == '&') ? 1 : 0;

        //printf("Conditional Command '%s' is created. \n", CondCmd);
        fill_cond_cmd(ShellCmd->CondCmd[CountCondCmd], CondCmd, BackGr);

        free(CondCmd);
    }

    return ShellCmd;
}

int clear_structs(struct ShellCommand* ShellCmd) {
    int CCondCmd = 0;
    int CCmd;
    int CSimpleCmd;
    int CArg;
    struct ShellCommand* ShellCmd2;
    struct Command* Cmd;
    while (ShellCmd->CondCmd[CCondCmd] != NULL) {
        CCmd = 0;
        while (ShellCmd->CondCmd[CCondCmd]->Cmd[CCmd] != NULL) {
            CSimpleCmd = 0;
            Cmd = ShellCmd->CondCmd[CCondCmd]->Cmd[CCmd];
            while (Cmd->SimpleCmd[CSimpleCmd] != NULL) {
                CArg = 0;
                while (Cmd->SimpleCmd[CSimpleCmd]->Arg[CArg] != NULL) {
                    free(Cmd->SimpleCmd[CSimpleCmd]->Arg[CArg]);
                    CArg++;
                }
                free(Cmd->SimpleCmd[CSimpleCmd]->Arg);
                free(Cmd->SimpleCmd[CSimpleCmd]->Name);
                free(Cmd->SimpleCmd[CSimpleCmd]);
                CSimpleCmd++;
            }
            free(Cmd->SimpleCmd);
            if (Cmd->Bracket != NULL) {
                ShellCmd2 = malloc(sizeof(struct ShellCommand));
                ShellCmd2->CondCmd = malloc(2*sizeof(struct ConditionalCommand*));
                ShellCmd2->CondCmd[0] = Cmd->Bracket;
                ShellCmd2->CondCmd[1] = NULL;
                clear_structs(ShellCmd2);
            }
            if (Cmd->Input != NULL) free(Cmd->Input);
            if (Cmd->Output != NULL) free(Cmd->Output);
            if (Cmd->OutputSave != NULL) free(Cmd->OutputSave);
            free(ShellCmd->CondCmd[CCondCmd]->Cmd[CCmd]);
            CCmd++;
        }
        free(ShellCmd->CondCmd[CCondCmd]->Cmd);
        free(ShellCmd->CondCmd[CCondCmd]);
        CCondCmd++;
    }
    free(ShellCmd->CondCmd);
    free(ShellCmd);

    return 0;
}

int main() {
    char* InputString;
    struct ShellCommand* ShellCmd;

    while (1) {
        printf("\n[my_shell]$ ");

        InputString = get_inpstr();

        ShellCmd = fill_struct(InputString);

        execute_shellcmd(ShellCmd);

        clear_structs(ShellCmd);

        free(InputString);
    }

    return 0;
}
