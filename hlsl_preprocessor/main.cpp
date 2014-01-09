#include "mojoshader.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <stdarg.h>
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <vector>
#include "Shlwapi.h"

std::vector<std::string> include_paths;
std::vector<std::string> define_strings;

std::string Format(const char * fmt, ...)
{
	va_list args;

	char szBuffer[1024];
	va_start(args, fmt);
	int r = vsnprintf(szBuffer, sizeof(szBuffer), fmt, args);
	va_end(args);

	return szBuffer;
}

bool starts_with(const char *s, const char *t) {
	while ((*s == *t) && *t) { ++s; ++t; }
	return (*t == '\0');
}

static void ReplaceString(std::string& target, const std::string& search,
						  const std::string& replace, size_t startPos)
{
	if (search.empty())
		return;

	std::string::size_type p = startPos;
	while ((p = target.find (search, p)) != std::string::npos)
	{
		target.replace (p, search.size (), replace);
		p += replace.size ();
	}
}

std::vector<std::string> SplitString(const std::string& str, const std::string& separator, bool trimEmpty)
{
	std::vector<std::string> result;
	std::string::size_type prev = 0, pos;
	while(prev < str.length() && (pos = str.find_first_of(separator, prev)) != std::string::npos)
	{
		if(pos > prev || !trimEmpty)
			result.push_back(str.substr(prev, pos - prev));
		prev = pos + 1;
	}
	if(prev < str.length() || !trimEmpty)
		result.push_back(str.substr(prev, str.length() - prev));
	return result;
}

void BuildDefine(MOJOSHADER_preprocessorDefine& define, const std::string &source)
{
	std::vector<std::string> tokens = SplitString(source, "=", true);
	if (tokens.size() != 2)
		throw std::runtime_error("Invalid preprocessor define");

	char* id = new char[tokens[0].length() + 1];
	char* def = new char[tokens[1].length() + 1];
	strcpy(id, tokens[0].c_str());
	strcpy(def, tokens[1].c_str());
	define.identifier = id;
	define.definition = def;
}

static bool ReadStringFromFile(const char* pathName, std::string& output)
{
	FILE* file = fopen( pathName, "rb" );
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length < 0)
	{
		fclose( file );
		return false;
	}

	output.resize(length);
	size_t readLength = fread(&*output.begin(), 1, length, file);

	fclose(file);

	if (readLength != length)
	{
		output.clear();
		return false;
	}

	ReplaceString(output, "\r\n", "\n", 0);

	return true;
}

HANDLE TryOpen(const char *fname) {
	HANDLE handle = INVALID_HANDLE_VALUE;
	for (auto it = include_paths.begin(); it != include_paths.end(); ++it) {
		char lpath[MAX_PATH];
		PathCombineA(lpath, it->c_str(), fname);

		WCHAR wpath[MAX_PATH];
		if (!MultiByteToWideChar(CP_UTF8, 0, lpath, -1, wpath, MAX_PATH))
			return 0;

		const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		handle = CreateFileW(wpath, FILE_GENERIC_READ, share,
										  NULL, OPEN_EXISTING, NULL, NULL);

		if (handle != INVALID_HANDLE_VALUE) break;
	}

	return handle;
}

int include_open(MOJOSHADER_includeType inctype,
                                     const char *fname, const char *parent,
                                     const char **outdata,
                                     unsigned int *outbytes,
                                     MOJOSHADER_malloc m, MOJOSHADER_free f,
                                     void *d)
{
	const HANDLE handle = TryOpen(fname);
    if (handle == INVALID_HANDLE_VALUE) {
		std::cerr << "Could not open " << fname << std::endl;
        return 0;
	}

    const DWORD fileSize = GetFileSize(handle, NULL);
    if (fileSize == INVALID_FILE_SIZE)
    {
        CloseHandle(handle);
        return 0;
    } // if

    char *data = (char *) m(fileSize, d);
    if (data == NULL)
    {
        CloseHandle(handle);
        return 0;
    } // if

    DWORD readLength = 0;
    if (!ReadFile(handle, data, fileSize, &readLength, NULL))
    {
        CloseHandle(handle);
        f(data, d);
        return 0;
    } // if

    CloseHandle(handle);

    if (readLength != fileSize)
    {
        f(data, d);
        return 0;
    } // if
    *outdata = data;
    *outbytes = fileSize;
    return 1;
}

void include_close(const char *data, MOJOSHADER_malloc m,
                                       MOJOSHADER_free f, void *d)
{
	f((void *) data, d);
}

void Preprocess(const std::string& filename, const std::string& target)
{
	std::string source;
	if (!ReadStringFromFile(filename.c_str(), source))
		throw std::runtime_error("Could not read source file");

	std::vector<MOJOSHADER_preprocessorDefine> defines(define_strings.size());
	for (unsigned int i = 0; i < define_strings.size(); i++) {
		BuildDefine(defines[i], define_strings[i]);
	}

	MOJOSHADER_preprocessorDefine *ppdefs = NULL;
	if (!defines.empty())
		ppdefs = &defines[0];

	const MOJOSHADER_preprocessData *pd = MOJOSHADER_preprocess(
		filename.c_str(), source.c_str(), source.length(),
		ppdefs, defines.size(),
		include_open, include_close,
		0, 0, 0);

	//free macros
	for (auto it = defines.begin(); it != defines.end(); ++it) {
		delete[] it->definition;
		delete[] it->identifier;
	}

	if (pd->error_count > 0)
	{
		for (int i = 0; i < pd->error_count; i++)
		{
			fprintf(stderr, "%s:%d: ERROR: %s\n",
				pd->errors[i].filename ? pd->errors[i].filename : "???",
				pd->errors[i].error_position,
				pd->errors[i].error);
		}

		throw std::runtime_error("Failed to preprocess");
	}
	else if (pd->output != NULL)
	{
		const char* outfile = target.c_str();
		FILE* io = fopen(target.c_str(), "wb");
		if (!io)
			throw std::runtime_error("File open failed");
        const int len = pd->output_len;

		std::cout << pd->output << std::endl;

        if ((len) && (fwrite(pd->output, len, 1, io) != 1))
			throw std::runtime_error(Format(" ... fwrite('%s') failed.\n", outfile).c_str());
        else if ((outfile != NULL) && (fclose(io) == EOF))
			throw std::runtime_error(Format(" ... fclose('%s') failed.\n", outfile).c_str());
	}
}

void PrintUsage()
{
	std::cout << "Usage: [options] <source> <target>\n";
	std::cout << "Options:\n";
	std::cout << "\t-IDIR, append DIR to include path (windows style)\n";
	std::cout << "\tFor paths with spaces, enclose the entire -IDIR in quotes.\n";
	std::cout << "\t-DNAME=VALUE, add macro NAME with value of VALUE" << std::endl;
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		PrintUsage();
		return 1;
	}

	include_paths.push_back(".");

	std::string source;
	std::string target;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
				std::string argstr(argv[i]);
			if (starts_with(argstr.c_str(), "-I")) {
				include_paths.push_back(argstr.substr(2));
			} else if (starts_with(argstr.c_str(), "-D")) {
				define_strings.push_back(argstr.substr(2));
			}
		} else {
			if (source.empty())
				source = argv[i];
			else if (target.empty())
				target = argv[i];
		}
	}

	if (source.empty() || target.empty())
	{
		std::cerr << "Both source and target must be specified" << std::endl;
		return 1;
	}

	if (source.compare(target) == 0)
	{
		std::cerr << "Source and target cannot be the same" << std::endl;
		return 1;
	}

	if (include_paths.empty())
	{
		std::cerr << "No known include paths" << std::endl;
		return 1;
	}

	try {
		Preprocess(source, target);
		std::cout << "Preprocessed " << source.c_str() << " to " << target.c_str() << std::endl;
	} catch (std::runtime_error& ex) {
		std::cerr << "Failed to preprocess " << source.c_str() << std::endl;
		std::cerr << ex.what() << std::endl;
		return 1;
	}

	return 0;
}