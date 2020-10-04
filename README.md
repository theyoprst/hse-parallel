# Семинар про параллельность

## Как скачать этот код

У вас должен быть [установлен git](https://git-scm.com/book/ru/v2/%D0%92%D0%B2%D0%B5%D0%B4%D0%B5%D0%BD%D0%B8%D0%B5-%D0%A3%D1%81%D1%82%D0%B0%D0%BD%D0%BE%D0%B2%D0%BA%D0%B0-Git). В консоли:

```sh
git clone https://github.com/theyoprst/hse-parallel.git
```

## Как собрать код

Код в двух функциях использует расширение стандартной библиотеки – Parallel STL. Из коробки это
доступно только в Microsoft Visual Studio последних версий.

Ниже про ситуацию с Parallel STL на разных платформах.

### Windows - инструкции с лекции

Желательно иметь последнюю версию Microsoft Visual Studio. Компилировать с флагом `/std:c++latest`.
Сначала нужно создать проект, затем добавить в этот проект исходные файлы, затем настроить версию стандарта
в настройках проекта.
Альтернативно можно прямо в Visual Studio скачать этот репозиторий, и все проекты появятся: File -> Clone Repository.
Если хочется компилировать из консоли, то тут 2 варианта:
1. Через специальную консоль, доступную из Пуска: https://stackoverflow.com/a/7865708
2. Написать свой батник, который сначала вызывает vsvsar32.bat, см [build-windows.bat](sem1/build-windows.bat).

### Ubuntu - инструкции с лекции

```sh
sudo apt install g++-9 libstdc++-9-dev libtbb-dev
g++-9 seminar_test_func.cpp --std=c++17 –ltbb –lpthread
```

### Ubuntu - более сложные инструкции

Инструкции с лекции могут не сработать, тогда есть более сложный путь.

1. Устанавливаем последнюю версию g++:

    Вот хороший гайд: https://tuxamito.com/wiki/index.php/Installing_newer_GCC_versions_in_Ubuntu
    В итоге вам нужно добиться, чтобы команда `g++ --version` выдавала версию 10:

    ```sh
    % g++ --version
    g++ (Ubuntu 10-20200416-0ubuntu1) 10.0.1 20200416 (experimental) [master revision 3c3f12e2a76:dcee354ce56:44b326839d864fc10c459916abcc97f35a9ac3de]
    ```

2. Выкачиваем исходники библиотеки Intel Threading Build Blocks в папку `~/tbbsrc`:

    ```sh
    git clone https://github.com/oneapi-src/oneTBB.git ~/tbbsrc
    ```

3. Собираем библиотеку и наш исходник [по такой инструкции](https://stackoverflow.com/questions/10726537/how-to-install-tbb-from-source-on-linux-and-make-it-work/10769131#10769131). Некоторые комментарии по инструкции
    * Установку переменных окружения лучше продублировать в `~/.bashrc`, чтобы при
      запуске нового терминала они не пропадали. Пример:

      ```sh
      export TBB_INSTALL_DIR=$HOME/tbbsrc
      export TBB_INCLUDE=$TBB_INSTALL_DIR/include
      export TBB_LIBRARY_RELEASE=$TBB_INSTALL_DIR/build/linux_intel64_gcc_cc10.0.1_libc2.31_kernel5.4.0_release
      ```

    * Порядок следования ключей компиляции важен:
      * сначала путь к подключаемым исходникам (`-I`)
      * затем пути к подключаемым динамическим библиотекам (`-Wl`, `-L`)
      * затем названия подключаемых динамических библиотек (`-l`)

    * В отличие от оригинальной инструкции, нам понадобится еще подключить библиотеку
        libpthread (`-lpthread`)

    * У меня получилось без `-Wl`, см. файл [sem1/build-ubuntu.sh](build-ubuntu.sh).

### Macos

Вариантов не найдено. Можно завести виртуалку с Ubuntu или Windows.
