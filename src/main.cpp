// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <iostream>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <ftw.h>
#include <pwd.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <queue>

static int dir_call_back(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf);

static int file_call_back(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf);

static std::string get_user_name(uid_t uid);

static std::string get_permission(mode_t mode);

static unsigned char get_file_type(int tflag, mode_t mode, bool *access);

static std::string get_date_time(const timespec &timespec);

struct list_el_t {
    bool is_dir;
    std::string f_path;
    std::string to_print;
};

static std::vector<list_el_t> el_to_sort{};
static std::queue<std::string> dirs_queue{};

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
    struct stat init_path_stat{};
    if (stat(path.data(), &init_path_stat) == -1) {
        std::cerr << "Error: fail to access '" << path << "'" << std::endl;
        return EXIT_FAILURE;
    }
    if (!S_ISDIR(init_path_stat.st_mode)) {
        if (nftw(path.data(), file_call_back, 20, FTW_PHYS | FTW_ACTIONRETVAL) == -1) {
            std::cerr << "Error: fail to recursively go through path tree!" << std::endl;
            return EXIT_FAILURE;
        } else {
            return EXIT_SUCCESS;
        }
    }
    dirs_queue.push(std::move(path));
    std::string tmp_dir_path;
    while (!dirs_queue.empty()) {
        tmp_dir_path = dirs_queue.front();
        if (nftw(dirs_queue.front().data(), dir_call_back, 20, FTW_PHYS | FTW_ACTIONRETVAL) == -1) {
            return EXIT_FAILURE;
        }
        dirs_queue.pop();
        std::sort(el_to_sort.begin(), el_to_sort.end(),
                  [](const list_el_t &el1, const list_el_t &el2) { return el1.f_path < el2.f_path; });
        std::cout << tmp_dir_path << ":" << std::endl;
        for (auto &list_el : el_to_sort) {
            if (list_el.is_dir)
                dirs_queue.push(std::move(list_el.f_path));
            std::cout << list_el.to_print << std::endl;
        }
        el_to_sort.clear();
        std::cout << std::endl;
    }
    return EXIT_SUCCESS;
}

static int dir_call_back(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (ftwbuf->level == 0)
        return FTW_CONTINUE;
    std::string user = get_user_name(sb->st_uid);
    std::string file_name = basename(fpath);
    std::string permissions = get_permission(sb->st_mode);
    std::string date_time = get_date_time(sb->st_mtim);
    bool access = true;
    unsigned char f_type = get_file_type(tflag, sb->st_mode, &access);
    bool is_dir = false;
    if (!access) {
        std::cerr << "Error: For path '" << fpath << "' permission denied!" << std::endl;
        return FTW_CONTINUE;
    } else if (f_type == '/') {
        if (file_name == "." || file_name == "..")
            return FTW_CONTINUE;
        else {
            is_dir = true;
        }
    }

    el_to_sort.emplace_back(
            list_el_t{is_dir, fpath,
                      (boost::format("%s %10s %12s  %s %c%s") % permissions % user
                       % (intmax_t) sb->st_size % date_time % f_type % file_name).str()}
    );
    if (is_dir)
        return FTW_SKIP_SUBTREE;
    else
        return FTW_CONTINUE;
}

static int file_call_back(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    std::string user = get_user_name(sb->st_uid);
    std::string file_name = basename(fpath);
    std::string permissions = get_permission(sb->st_mode);
    std::string date_time = get_date_time(sb->st_mtim);
    bool access = true;
    unsigned char f_type = get_file_type(tflag, sb->st_mode, &access);
    if (!access) {
        std::cerr << "Error: For path '" << fpath << "' permission denied!" << std::endl;
        return FTW_STOP;
    }
    std::cout << boost::format("%s %10s %12s  %s %c%s") % permissions % user
                 % (intmax_t) sb->st_size % date_time % f_type % file_name << std::endl;
    return FTW_STOP;
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

static unsigned char get_file_type(int tflag, mode_t mode, bool *access) {
    switch (tflag) {
        case FTW_D:
            return '/';
        case FTW_DNR:
            *access = false;
            return '/';
        case FTW_SL:
            return '@'; // may be '?' for FTW_SLN
        case FTW_SLN:
            *access = false;
            return '@';
        case FTW_NS:
            *access = false;
            return '?';
        default:
            if (S_ISFIFO(mode))
                return '|';
            else if (S_ISSOCK(mode))
                return '=';
            else if (S_ISREG(mode)) {
                if (S_IXUSR & mode)
                    return '*';
                return ' ';
            } else
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