// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

// Remember to include ALL the necessary headers!
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <ftw.h>
#include <pwd.h>
#include <unistd.h>
#include <filesystem>

// TODO: Critical ==> Якщо директорію
//      вивести її вміст
//      посортований за іменем
//      потім, рекурсивно -- вміст кожної із її піддиректорій, теж в алфавітному порядку
//      Малі літери вважаються меншими за великі -- мають йти на початку.

// TODO: Попередження ==> Якщо переданого шляху не існує -- повідомити про помилку, з використанням stderr та завершити виконання.

static int call_back(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf);

int main(int argc, char **argv) {
    if (argc > 2) {
        std::cerr << "Error: to many arguments! See 'myrls --help' for more info..." << std::endl;
        return EXIT_FAILURE;
    }
    std::string path;

    {
        namespace po = boost::program_options;
        po::options_description visible("Supported options");
        visible.add_options()
                ("help,h", "Print this help message.");

        po::options_description hidden("Hidden options");
        hidden.add_options()
                ("path", po::value<std::string>(&path)->default_value("./"), "Variable path.");

        po::positional_options_description p;
        p.add("path", 1);

        po::options_description all("All options");
        all.add(visible).add(hidden);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(all).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << "Usage:\n  myrls [file_path]\nDescription:\n  Recursively list directories" << visible
                      << std::endl;
            return EXIT_SUCCESS;
        }
    }
    if (access(path.data(), F_OK) == -1) {
        std::cerr << "Error: file '" << path << "' do not exist!";
        return EXIT_FAILURE;
    }
    if (nftw(path.data(), call_back, 20, FTW_PHYS | FTW_ACTIONRETVAL) == -1) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static std::string get_user_name(uid_t uid) {
    static std::unordered_map<__uid_t, std::string> users_by_uid{};
    auto const &user_it = users_by_uid.find(uid);
    if (user_it == users_by_uid.end()) {
        const auto user_name = getpwuid(uid)->pw_name;
        users_by_uid[uid] = user_name;
        return user_name;
    } else {
        return user_it->second;
    }
}

static std::string get_permission(mode_t mode) {
    constexpr char perm_str[] = "-xw r";
    char res[9];
    uint16_t triad_perm;
    uint8_t triad_cursor;
    mode &= 0777u;
    for (int8_t i = 2; i >= 0; --i) {
        triad_perm = mode >> (3u * i);
        triad_cursor = 3 * (2 - i);
        res[0 + triad_cursor] = perm_str[triad_perm & 0b100u];
        res[1 + triad_cursor] = perm_str[triad_perm & 0b010u];
        res[2 + triad_cursor] = perm_str[triad_perm & 0b001u];
    }
    return res;
}

static unsigned char get_file_type(int tflag, mode_t mode) {
    switch (tflag) {
        case FTW_F:
            return ' ';
        case FTW_D:
        case FTW_DNR:
            return '/';
        case FTW_SL:
        case FTW_SLN:
            return '@'; // may be '?' for FTW_SLN
        case FTW_NS:
            return '\0';
        default:
            if (S_ISFIFO(mode))
                return '|';
            else if (S_ISSOCK(mode))
                return '=';
            else
                return '?';
    }
}

static std::string get_date_time(const timespec &timespec) {
    struct tm time{};
    char buf[20];
    tzset();
    localtime_r(&(timespec.tv_sec), &time);
    strftime(buf, 20, "%F %T", &time);
    return buf;
}

int call_back(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    std::string user = get_user_name(sb->st_uid);
    std::string file_name = basename(fpath);
    std::string permissions = get_permission(sb->st_mode);
    std::string date_time = get_date_time(sb->st_mtim);
    unsigned char f_type = get_file_type(tflag, sb->st_mode);

    if (!f_type) {
        std::cerr << "Error: For path '" << fpath
                  << "'. The stat() function failed on the object because of lack of appropriate permission. On depth level: "
                  << ftwbuf->level << std::endl;
        return FTW_CONTINUE;
    } else if (f_type == '/') {
        if (file_name == "." || file_name == "..")
            return FTW_CONTINUE;
//        else
//            return FTW_SKIP_SUBTREE;
    }

    std::cout << boost::format("%s %s %12s  %s %c%s\n") % permissions % user % (intmax_t) sb->st_size % date_time
                 % f_type % file_name;
    return FTW_CONTINUE;  /* To tell nftw() to continue */
}
