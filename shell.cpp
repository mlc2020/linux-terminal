#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <time.h>

using namespace std;

void trim(vector<string>& parts) {
    for (int i = 0; i < parts.size(); ++i) {
        int j = 1;
        while (j < parts[i].size()) {
            if (j == 1 && parts[i].at(j-1) == ' ') {
                parts[i].erase(parts[i].begin());
                continue;
            } else if (parts[i].at(j-1) == ' ' && parts[i].at(j) == ' ') {
                parts[i].erase(parts[i].begin() + j - 1);
                continue;
            }
            ++j;
        }
        if (parts[i].at(j-1) == ' ') {
            parts[i].erase(j-1);
        }
    }
}

vector<string> split(string line, char separator = ' ') {
    vector<string> parsedline;
    string part = "";
    int i = 0;
    int dcounter = 0;//different double and single quote counters
    int scounter = 0;
    while (i < line.length()) {
        if (line[i] == '\"') {
            dcounter += 1;
            part += line[i];
        } else if (line[i] == '\'') {
            scounter += 1;
            part += line[i];
        } else if (dcounter % 2 != 0 || scounter % 2 != 0) {
            part += line[i];
        } else if (dcounter % 2 == 0 && scounter % 2 == 0 && line[i] == separator && part != "") {
            parsedline.push_back(part);
            part = "";
            dcounter = 0;
            scounter = 0;
        } else if (line[i] == separator && part != "") {
            //Ex. "-ls     -w" will not push_back if delimiter is ' '
            parsedline.push_back(part);
            part = "";
        } else if (line[i] != separator) {//Ex. "-ls     -w" will ignore the spaces if delimiter is NOT ' '
            part += line[i];
        }
        ++i;
    }
    if (part != "") {
        parsedline.push_back(part);
    }
    return parsedline;
}

char** vec_to_char_array(vector<string>& parts) {
    char** result = new char* [parts.size() + 1];
    for (int part = 0; part < parts.size(); ++part) {
        if (parts[part].at(0) == '\"' || parts[part].at(0) == '\'') {
            parts[part].erase(parts[part].begin());
            parts[part].erase(parts[part].begin() + parts[part].size() - 1);
        }
        result[part] = (char*) parts[part].c_str();
    }
    result[parts.size()] = NULL;
    return result;
}

int main() {
    dup2(0, 3);
    vector<int> bgprocesses;
    vector<string> previous_directories;
    char buf[200];
    string username = getenv("USER");
    while (true) {
        for (int id = 0; id < bgprocesses.size(); ++id) {
            if (waitpid(bgprocesses[id], 0, WNOHANG) < 0) {
                ;
            } else {
                bgprocesses.erase(bgprocesses.begin() + id);
                --id;
            }
        }
        dup2(3,0);
        bool bg = false;
        time_t rawtime;
        struct tm * timeinfo;
        time (&rawtime);
        timeinfo = localtime(&rawtime);
        cout << username << ", " << asctime(timeinfo) << ">";
        string inputline;
        getline(cin, inputline);
        if (inputline == string("exit")) {
            cout << "End of shell\n";
            break;
        }

        if (inputline == string("echo $PATH")) {
            cout << "Current Directory: " << getcwd(buf, sizeof(buf)) << endl;
        }

        vector<string> parts = split(inputline, '|');
        trim(parts);

        if (parts[0].at(parts[0].size() - 1) == '&') {//currently {sleep, 5, &}
            bg = true;
        }

        vector<string> directories = split(parts[0]);
        
        if (directories[0] == "cd") {
            if (directories[1] == "-") {
                char* prevdirectory = (char*) previous_directories[previous_directories.size() - 1].c_str();
                chdir(prevdirectory);
            } else {
                previous_directories.push_back(getcwd(buf, sizeof(buf)));
                chdir((char*) directories[1].c_str());
            }
            string cd = getcwd(buf, sizeof(buf));
            cout << "Current Directory: " << cd << endl;
            continue;
        }

        for (int process = 0; process < parts.size(); ++process) {
            int fds[2];
            int fdwrite, fdread;
            pipe(fds);
            int cid = fork();
            if (!cid) { //CHILD PROCESS
                if (process < parts.size() - 1) {
                    dup2(fds[1], 1);
                }
                vector<string> argvec = split(parts[process]);
                for (int arg = 0; arg < argvec.size(); ++arg) { //IO REDIRECT HANDLING
                    if (argvec[arg] == ">") {
                        fdwrite = open(argvec[arg + 1].c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                        dup2(fdwrite, 1);
                        argvec.erase(argvec.begin() + arg);
                        argvec.erase(argvec.begin() + arg);
                        --arg;
                    }
                    if (argvec[arg] == "<") {
                        fdread = open(argvec[arg + 1].c_str(), O_RDONLY);
                        dup2(fdread, 0);
                        argvec.erase(argvec.begin() + arg);
                        argvec.erase(argvec.begin() + arg);
                        --arg;
                    }
                    if (argvec[arg] == "&") {//currently {sleep, 5, &}
                        argvec.erase(argvec.begin() + arg);//now {sleep, 5}
                        --arg;
                    }
                }
                char** args = vec_to_char_array(argvec);
                execvp(args[0], args);
            } else { //PARENT PROCESS
                if (bg == true) {
                    bgprocesses.push_back(cid);
                }
                else if (process == parts.size() - 1) {
                    waitpid(cid, 0, 0);
                }
                close(fdwrite);
                close(fdread);
                dup2(fds[0], 0);
                close(fds[1]);
            }
        }
    }
}