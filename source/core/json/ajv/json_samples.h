//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#ifndef __AJV_SAMPLES_H_DEFINED
#define __AJV_SAMPLES_H_DEFINED

// #define AJV_INCLUDE_SAMPLES
// #define AJV_CONFIG_PARSE_CLI
// #define AJV_CONFIG_CHECK_CLI

#ifdef AJV_INCLUDE_SAMPLES

#include <ajv.h>

AJV_FN_NO_INLINE_(void) ajv_sample_array_view_1()
{
    auto json = "[\"Sunday\", \"Monday\", \"Tuesday\", \"Wednesday\", \"Thursday\", \"Friday\", \"Saturday\"]";

    auto parsed = ajv::JsonParser::Parse(json, strlen(json));
    assert(parsed.IsOk());

    size_t size;
    const char* ptr = parsed[0].AsStringPtr(&size); // AsStringPtr returns pointer into original string
    auto check = ptr != nullptr && size == strlen("Sunday"); assert(check);
    check = strncmp(ptr, "Sunday", size) == 0; assert(check);

    // use operator[] to access arrays
    check = strncmp(parsed[6].AsStringPtr(&size), "Saturday", size) == 0; assert(check);
    check = strncmp(parsed[5].AsStringPtr(&size), "Friday", size) == 0; assert(check);
    check = strncmp(parsed[4].AsStringPtr(&size), "Thursday", size) == 0; assert(check);
    check = strncmp(parsed[3].AsStringPtr(&size), "Wednesday", size) == 0; assert(check);
    check = strncmp(parsed[2].AsStringPtr(&size), "Tuesday", size) == 0; assert(check);
    check = strncmp(parsed[1].AsStringPtr(&size), "Monday", size) == 0; assert(check);
}

AJV_FN_NO_INLINE_(void) ajv_sample_array_view_2()
{
    auto json = "[\"Sunday\", \"Monday\", \"Tuesday\", \"Wednesday\", \"Thursday\", \"Friday\", \"Saturday\"]";

    auto parsed = ajv::JsonParser::Parse(json, strlen(json));
    assert(parsed.IsOk());

    size_t size = 0;
    auto item = parsed[0];
    auto ptr = item.AsStringPtr(&size);
    auto check = ptr != nullptr && size == strlen("Sunday"); assert(check);
    check = strncmp(item.AsStringPtr(&size), "Sunday", size) == 0; assert(check);

    // use operator++ to iterate thru an array
    check = strncmp(item++.AsStringPtr(&size), "Monday", size) == 0; assert(check);
    check = strncmp(item++.AsStringPtr(&size), "Tuesday", size) == 0; assert(check);
    check = strncmp(item++.AsStringPtr(&size), "Wednesday", size) == 0; assert(check);
    check = strncmp(item++.AsStringPtr(&size), "Thursday", size) == 0; assert(check);
    check = strncmp(item++.AsStringPtr(&size), "Friday", size) == 0; assert(check);
    check = strncmp(item++.AsStringPtr(&size), "Saturday", size) == 0; assert(check);
}

AJV_FN_NO_INLINE_(void) ajv_sample_array_view_3()
{
    auto json = "[\"Sunday\", \"Monday\", \"Tuesday\", \"Wednesday\", \"Thursday\", \"Friday\", \"Saturday\"]";

    auto parsed = ajv::JsonParser::Parse(json, strlen(json));
    assert(parsed.IsOk());

    auto str = parsed[0].AsString(); // AsString returns new std::string's with new allocated memory
    auto check = str.size() == strlen("Sunday"); assert(check);
    check = str == "Sunday"; 

    check = parsed[1].AsString() == "Monday"; assert(check);
    check = parsed[2].AsString() == "Tuesday"; assert(check);
    check = parsed[3].AsString() == "Wednesday"; assert(check);
    check = parsed[4].AsString() == "Thursday"; assert(check);
    check = parsed[5].AsString() == "Friday"; assert(check);
    check = parsed[6].AsString() == "Saturday"; assert(check);
}

AJV_FN_NO_INLINE_(void) ajv_sample_object_view()
{
    auto json = "{\"name\":\"alice\", \"email\":\"alice@example.com\"}";

    auto parsed = ajv::JsonParser::Parse(json, strlen(json)); // light weight parse, using caller memory
    assert(parsed.IsOk());

    size_t size;
    const char* ptr = parsed["name"].AsStringPtr(&size); // use operator[] to access objects
    auto check = ptr != nullptr && size == strlen("alice"); assert(check);

    check = strncmp(ptr, "alice", size) == 0; assert(check);
    check = strncmp(parsed["email"].AsStringPtr(&size), "alice@example.com", size) == 0; assert(check);
}

AJV_FN_NO_INLINE_(void) ajv_sample_object_object_view()
{
    auto json = "{"
        "\"buyer\": {\"name\":\"your name\", \"email\":\"you@github.com\"},"
        "\"seller\": {\"name\":\"alice\", \"email\":\"alice@example.com\"}"
    "}";

    auto parsed = ajv::JsonParser::Parse(json, strlen(json));
    auto check = parsed.IsOk(); assert(check);

    auto buyerName = parsed["buyer"]["name"]; // use operator[] repeatedly to access sub objects
    auto sellerName = parsed["seller"]["name"];

    size_t size = 0;
    check = strncmp(buyerName.AsStringPtr(&size), "your name", size) == 0; assert(check);
    check = strncmp(sellerName.AsStringPtr(&size), "alice", size) == 0; assert(check);
}

AJV_FN_NO_INLINE_(const char*) ajv_sample_file_base_get(const char* fileName)
{
    auto base = strrchr(fileName, '/');
    if (base != nullptr) return base + 1;

    base = strrchr(fileName, '\\');
    if (base != nullptr) return base + 1;

    return fileName;
}

AJV_FN_NO_INLINE_(const char*) ajv_sample_file_read(const char* fileName, size_t* size)
{
    if (fileName == nullptr || size == nullptr) return nullptr;

    FILE* file;
    if (fopen_s(&file, fileName, "rb") != 0) return nullptr;

    if (fseek(file, 0, SEEK_END) != 0) return nullptr;
    *size = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0) return nullptr;

    char* data = (char*)malloc(*size + 1);
    if (fread(data, 1, *size, file) != *size)
    {
        free(data);
        return nullptr;
    }

    fclose(file);

    data[*size] = '\0';
    return data;
}

AJV_FN_NO_INLINE_(int) ajv_sample_file_parse(const char* fileName)
{
    size_t size = 0;
    auto data = ajv_sample_file_read(fileName, &size);
    if (data == nullptr)
    {
        printf("error reading file '%s'\n", fileName);
        return -2;
    }

    auto parsed = ajv::json::Parse(data, size);
    auto ok = !parsed.IsError() && !parsed.IsEmpty();
    free((void*)data);

    return ok ? 0 : -1;
}

AJV_FN_NO_INLINE_(int) ajv_sample_file_parse_cli(int argc, char** argv)
{
    auto fileName = argc >= 2 ? argv[1] : "";
    if (fileName[0] == '\0')
    {
        auto program = ajv_sample_file_base_get(argv[0]);
        printf("%s JSONFILE\n", program);
        return -2;
    }

    auto result = ajv_sample_file_parse(fileName);
    auto parsed = result == 0 ? 'y' : 'n';

    printf("%c = parsed('%s'); result=%d\n", parsed, fileName, result);
    return result;
}

AJV_FN_NO_INLINE_(int) ajv_sample_file_check(const char* fileName, char expected)
{
    auto rv = ajv_sample_file_parse(fileName);
    auto parsed = rv == 0 ? 'y' : rv == -1 ? 'n' : '-';
    auto code = parsed == '-' ? rv
              : expected == '*' ? 0
              : parsed == expected ? 0
              : -1;

    printf("%c = parsed('%s'); expected='%c'; code=%d\n", parsed, fileName, expected, code);
    return code;
}

AJV_FN_NO_INLINE_(int) ajv_sample_file_check_cli(int argc, char** argv)
{
    auto fileName = argc >= 2 ? argv[1] : "";
    auto expected = argc < 3 ? '*'
                  : argv[2][0] == 'y' ? 'y'
                  : argv[2][0] == 'n' ? 'n'
                  : argv[2][0] == 'i' ? '*'
                  : argv[2][0] == '*' ? '*'
                  : '\0';

    if (fileName[0] == '\0' || expected == '\0')
    {
        auto program = ajv_sample_file_base_get(argv[0]);
        printf("%s JSONFILE [y/n/i/*]\n", program);
        return -2;
    }

    return ajv_sample_file_check(fileName, expected);
}

#ifdef AJV_CONFIG_PARSE_CLI
int main(int argc, char** argv)
{
    return ajv_sample_file_parse_cli(argc, argv);
}
#endif

#ifdef AJV_CONFIG_CHECK_CLI
int main(int argc, char** argv)
{
    return ajv_sample_file_check_cli(argc, argv);
}
#endif

#endif

#endif // __AJV_SAMPLES_H_DEFINED
