#include <iostream>
#include <string>

// put your canonicalizePath + isPathSafe here, or include the .hpp that declares them

#include <vector>
#include <string>

// Normalize things like:
//   "/var/www/../www/site/./index.html" → "/var/www/site/index.html"
//   "www/./sub/../file"                → "www/file"
static std::string canonicalizePath(const std::string &path)
{
    bool absolute = !path.empty() && path[0] == '/';

    std::vector<std::string> parts;
    std::string current;

    for (size_t i = 0; i <= path.size(); ++i)
    {
        char c = (i < path.size()) ? path[i] : '/';
        if (c == '/')
        {
            if (!current.empty())
            {
                if (current == ".")
                {
                    // ignore
                }
                else if (current == "..")
                {
                    if (!parts.empty())
                        parts.pop_back();
                }
                else
                {
                    parts.push_back(current);
                }
                current.clear();
            }
        }
        else
        {
            current += c;
        }
    }

    std::string result;
    if (absolute)
        result = "/";

    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i > 0 || absolute)
            result += "/";
        result += parts[i];
    }

    if (result.empty())
        return absolute ? "/" : ".";

    return result;
}

bool isPathSafe(const std::string& basePath, const std::string& fullPath)
{
    // Canonical base path
    std::string base = canonicalizePath(basePath);

    // Parent directory of the requested path
    std::string dirPart;
    std::string::size_type slash = fullPath.find_last_of('/');
    if (slash == std::string::npos)
        dirPart = fullPath;
    else if (slash == 0)
        dirPart = "/";
    else
        dirPart = fullPath.substr(0, slash);

    std::string parent = canonicalizePath(dirPart);

    if (base.empty() || parent.empty())
        return false;

    // 1) exact match: file directly under base
    if (parent == base)
        return true;

    // 2) parent is a subdirectory of base: base + "/" + ...
    if (parent.size() > base.size()
        && parent.compare(0, base.size(), base) == 0
        && parent[base.size()] == '/')
    {
        return true;
    }

    // otherwise: outside base or prefix trap like "/var/www_malicious"
    return false;
}


bool isPathSafe(const std::string& basePath, const std::string& fullPath);

struct Case
{
	std::string base;
	std::string full;
	bool        expected;
	const char* desc;
};

int main()
{
	Case cases[] =
	{
		// simple inside
		{ "/var/www", "/var/www/index.html", true,  "simple file in root" },
		{ "/var/www", "/var/www/img/logo.png", true, "subdir under root" },

		// with ../ but still inside
		{ "/var/www", "/var/www/img/../index.html", true, "normalize ../ but stays inside" },

		// traversal attempt
		{ "/var/www", "/var/www/../etc/passwd", false, "attempt to escape with .." },
		{ "/var/www", "/etc/passwd",            false, "completely outside base" },

		// prefix trap
		{ "/var/www", "/var/www_malicious/file.txt", false, "prefix path should NOT be allowed" },

		// relative-ish stuff if you ever pass that in
		{ "www", "www/index.html", true, "relative base and file" },
		{ "www", "www/../etc/passwd", false, "relative escape" }
	};

	const size_t n = sizeof(cases) / sizeof(cases[0]);
	bool all_ok = true;

	for (size_t i = 0; i < n; ++i)
	{
		bool got = isPathSafe(cases[i].base, cases[i].full);
		bool ok = (got == cases[i].expected);

		std::cout << (ok ? "[OK]   " : "[FAIL] ")
		          << cases[i].desc << "  base='" << cases[i].base
		          << "' full='" << cases[i].full
		          << "' -> got=" << got << " expected=" << cases[i].expected
		          << std::endl;

		if (!ok)
			all_ok = false;
	}

	return all_ok ? 0 : 1;
}
