/* MAGIC: creating a process is a kernel operation with its own address
   space, handles, and exit status - not a function call in the same image. */
#include <stdio.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
int main(void)
{
#if defined(_WIN32)
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    char cmd[] = "cmd.exe /C echo child says hello";
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    printf("child exit=%lu\n", (unsigned long)code);
#else
    pid_t pid = fork();
    if (pid == 0) {
        execlp("sh", "sh", "-c", "echo child says hello", (char *)NULL);
        _exit(127);
    }
    if (pid < 0) { perror("fork"); return 1; }
    int status = 0;
    waitpid(pid, &status, 0);
    printf("child exit=%d\n", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
#endif
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Starts a separate child process that prints a line, waits for it to
     finish, and reports its exit code. On Windows it uses CreateProcess;
     on POSIX it uses fork plus exec.

   Step by step
     1. Windows fills STARTUPINFO with the standard input/output/error
        handles so the child's cmd.exe writes to the same console. The
        command line is mutable because CreateProcess may modify it.
     2. CreateProcess creates a new process object and an initial thread.
        The child gets a copy-on-write address space and its own handle
        table; it does not share C globals with the parent.
     3. WaitForSingleObject(pi.hProcess, INFINITE) blocks until the child
        exits. GetExitCodeProcess retrieves its status, and both handles
        are closed to avoid leaks.
     4. On POSIX, fork duplicates the process; the child calls execlp to
        replace its image with sh -c "echo ...". The parent waitpid blocks
        and decodes the status with WIFEXITED/WEXITSTATUS.
     5. Both paths print the child's line and "child exit=0". The order is
        deterministic because the parent waits.

   Output
     "child says hello"
     "child exit=0"

   Why it matters
     Process creation is the foundation of shells, servers, build tools,
     and sandboxes. Windows creates the process and thread together;
     POSIX separates copy (fork) from replacement (exec), which allows
     redirection and privilege changes between the two. Understanding the
     split explains shells, file-descriptor inheritance, and why fork is
     expensive on a large address space.

   Caveats
     - Constructing a command string for a shell invites injection. Prefer
       an argument vector and avoid `sh -c` for untrusted input.
     - Handle inheritance is a security boundary: do not leak sockets,
       files, or tokens to a child that does not need them.
     - Windows command-line quoting rules are not POSIX shell rules; use
       CreateProcess with a carefully built command line.
     - A POSIX child that exits before the parent waits becomes a zombie
       until waitpid; SIGCHLD handling or double-fork avoids accumulation.
     - fork in a multithreaded program only duplicates the calling thread;
       locks held by other threads can remain locked in the child. Prefer
       posix_spawn in that situation.
     - Exit codes are small integers, not error messages; a child may
       also be killed by a signal.
   ============================================== */
