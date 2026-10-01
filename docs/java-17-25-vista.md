# Java 17–25 on Vista / Server 2008

## Cause and change

Java 25 initially exited with `Failed to load ...\bin\server\jvm.dll`. Loader snaps on Server 2008 identified the unresolved export `kernel32.K32GetModuleFileNameExA` in `KxBase.dll`. On NT 6.0, the corresponding process-status functions are exported from `psapi.dll` without the `K32` prefix. `KxBase` now forwards its `K32*` exports to those Vista PSAPI exports. The `K32GetProcessMemoryInfo` compatibility implementation remains in `KxBase`.

The change is based on API availability, not on a Java executable name, version, or fixed address. All 26 PSAPI forwarding targets were checked against the native x64 and x86 Server 2008 `psapi.dll` exports. Both x64 and x86 `KxBase.dll` are included in `Installer`.

## Enable a JDK

After updating VxKex with `Installer\install.bat`, run an elevated Windows PowerShell session:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\path\to\Installer\Enable-Java-JDK.ps1" -JdkRoot "C:\Program Files\Java\jdk-25.0.4.1"
```

`-JdkRoot` can also point to a directory whose immediate children are JDK installations. The script reads each JDK's `release` file, accepts Java 17 or newer, and enables VxKex with Windows 10 version reporting for every `bin\*.exe` launcher. The path to `KexCfg.exe` can be supplied with `-KexCfgPath` if it is not next to the script. IFEO configuration is keyed by executable basename in Windows, so the settings also affect other JDK installations that use the same launcher names.

## Server 2008 validation

| JDK | VM result |
| --- | --- |
| Temurin 17.0.20.1 | `java -version`, `javac -version`, compile and run a class: exit 0 |
| Oracle 21.0.12.1 | `java -version`, `javac -version`, compile and run a class: exit 0 |
| Oracle 25.0.4.1 | `java`, `javac`, `jshell` version checks; compile and run a class; create and run a JAR; `jlink --version`: exit 0 |

The generic configuration script registered 35, 36, and 37 launchers for the respective JDKs. The x86 `KxBase.dll` was built and deployed on Server 2008; an x86 probe resolved its `K32GetModuleFileNameExA` export through Vista PSAPI. A complete 32-bit JDK was not tested. Intermediate versions between 17, 21, and 25 were not individually tested.

PSAPI mapping: [Microsoft's `GetModuleFileNameExA` documentation](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmodulefilenameexa), [Microsoft's `EnumProcessModulesEx` documentation](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-enumprocessmodulesex).

## Gradle and VS Code Java projects

For Gradle projects, run this additional script **as the same Windows user who runs VS Code** (no elevation is needed):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\path\to\Installer\Enable-Java-Gradle.ps1" -JdkRoot "C:\Program Files\Java\jdk-25.0.4.1" -ToolchainJdkRoot "C:\Program Files\Java\jdk-17"
```

`-JdkRoot` chooses the JDK used by the Gradle daemon. `-ToolchainJdkRoot` is optional and registers another installed JDK for projects that compile against it. This keeps the Gradle runtime on Java 25 while a project can still use a Java 17 toolchain. The script backs up the user's VS Code settings, preserves existing settings, and appends one version-independent Java option to the user's `JAVA_TOOL_OPTIONS`. Sign out and back in before launching VS Code normally so new processes inherit that option. It does not change `JAVA_HOME` or `PATH`.

Gradle 9.7.1's `gradle-fileevents.dll` imports `ReadDirectoryChangesExW`, which [Microsoft introduced with Windows 10 version 1709](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesexw). `KxBase` now exports it. The ordinary `FILE_NOTIFY_INFORMATION` class delegates to Vista's `ReadDirectoryChangesW`. Vista cannot provide the extended information class, so that request returns `ERROR_NOT_SUPPORTED`; the script sets `--no-watch-fs` to [disable Gradle's optional file watching](https://docs.gradle.org/current/userguide/file_system_watching.html). Gradle also lists Vista/Server 2008 outside its [supported operating systems](https://docs.gradle.org/current/userguide/compatibility.html), so this is a VxKex compatibility configuration.

Java 17, 21 and 25 on Server 2008 did not wake Gradle's selector after data arrived. The script sets `java.nio.channels.spi.SelectorProvider=sun.nio.ch.WindowsSelectorProvider`, which made the selector probe and Gradle 9.7.1 work with Java 25. It also disables the VS Code Gradle extension's Build Server importer; its named-pipe connection timed out on this VM, while the standard Buildship importer succeeded.

Server 2008 verification with `GoodSystem-feature-develop`: Java 25 ran `gradlew help --no-watch-fs` successfully; the normal VS Code profile reported `CONFIGURE SUCCESSFUL`, discovered 37 Gradle tasks, and the Java language server logged `Workspace initialized` followed by `build jobs finished`. This verifies project import, not every task in the project's build. The project's own Java 17 toolchain remained registered.
