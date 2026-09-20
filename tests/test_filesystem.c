/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/filesystem.h"

#define MAX_ENTRIES 32

static char g_root[MAX_PATH];

static const char* path_in_root(const char* relative)
{
    static char paths[4][MAX_PATH * 2];
    static size_t next = 0;
    char* path = paths[next++ % 4];

    snprintf(path, sizeof(paths[0]), "%s/%s", g_root, relative);

#if defined(ROMANO_WIN)
    {
        char* c;

        for(c = path; *c != '\0'; c++)
            if(*c == '/')
                *c = '\\';
    }
#endif /* defined(ROMANO_WIN) */

    return path;
}

static bool write_file(const char* path, const void* data, size_t size)
{
    FILE* file = fopen(path, "wb");
    bool ok;

    if(file == NULL)
        return false;

    ok = fwrite(data, 1, size, file) == size;
    fclose(file);

    return ok;
}

static void build_tree(void)
{
    snprintf(g_root, sizeof(g_root), "%s", test_tmp_path("filesystem_tree"));

    fs_remove(g_root);

    TEST_ASSERT(fs_makedirs(path_in_root("sub/deep")));
    TEST_ASSERT(fs_makedirs(path_in_root("sub/empty_nested")));
    TEST_ASSERT(fs_makedirs(path_in_root("empty_dir")));
    TEST_ASSERT(write_file(path_in_root("a.txt"), "hello\n", 6));
    TEST_ASSERT(write_file(path_in_root("empty_file"), "", 0));
    TEST_ASSERT(write_file(path_in_root("sub/c.bin"), "\x00\x01\x02\xFF", 4));
    TEST_ASSERT(write_file(path_in_root("sub/deep/d.txt"), "deep", 4));
}

static void test_file_content(void)
{
    FileContent content;
    FileContent* heap_content;

    build_tree();

    TEST_ASSERT(fs_file_content_init(&content, path_in_root("a.txt"), false));
    TEST_CHECK_EQ_UINT(content.content_sz, 6);
    TEST_CHECK_EQ_STR(content.content, "hello\n");
    fs_file_content_release(&content);
    TEST_CHECK(content.content == NULL);

    heap_content = fs_file_content_new(path_in_root("sub/c.bin"), true);
    TEST_ASSERT(heap_content != NULL);
    TEST_CHECK_EQ_UINT(heap_content->content_sz, 4);
    TEST_CHECK_EQ_MEM(heap_content->content, "\x00\x01\x02\xFF", 4);
    fs_file_content_free(heap_content);

    TEST_ASSERT(fs_file_content_init(&content, path_in_root("empty_file"), true));
    TEST_CHECK_EQ_UINT(content.content_sz, 0);
    TEST_CHECK_EQ_STR(content.content, "");
    fs_file_content_release(&content);

    TEST_CHECK(!fs_file_content_init(&content, path_in_root("does_not_exist"), false));
    TEST_CHECK(fs_file_content_new(path_in_root("does_not_exist"), false) == NULL);
}

static void test_queries(void)
{
    char* cwd = NULL;
    size_t cwd_size = 0;

    build_tree();

    TEST_CHECK(fs_path_exists(g_root));
    TEST_CHECK(fs_path_exists(path_in_root("a.txt")));
    TEST_CHECK(!fs_path_exists(path_in_root("nope")));

    TEST_CHECK(fs_is_dir(path_in_root("sub")));
    TEST_CHECK(!fs_is_dir(path_in_root("a.txt")));
    TEST_CHECK(!fs_is_dir(path_in_root("nope")));
    TEST_CHECK(fs_is_file(path_in_root("a.txt")));
    TEST_CHECK(!fs_is_file(path_in_root("sub")));
    TEST_CHECK(!fs_is_file(path_in_root("nope")));

    TEST_ASSERT(fs_get_cwd(&cwd, &cwd_size));
    TEST_CHECK(cwd != NULL && cwd_size == strlen(cwd) && cwd_size > 0);
    free(cwd);
}

static void test_parent_dir(void)
{
    char buffer[64];
    char* parent;

    TEST_CHECK_EQ_UINT(fs_parent_dir("a/b/c.txt"), 3);
    TEST_CHECK_EQ_UINT(fs_parent_dir("a/b/"), 3);
    TEST_CHECK_EQ_UINT(fs_parent_dir("C:\\dir\\file"), 6);
    TEST_CHECK_EQ_UINT(fs_parent_dir("/file"), 1);
    TEST_CHECK_EQ_UINT(fs_parent_dir("file"), 0);
    TEST_CHECK_EQ_UINT(fs_parent_dir(""), 0);

    parent = fs_parent_dir_new("/usr/local/lib");
    TEST_CHECK_EQ_STR(parent, "/usr/local");
    free(parent);

    TEST_CHECK_EQ_UINT(fs_parent_dir_init("/usr/local/lib", buffer, sizeof(buffer)), 0);
    TEST_CHECK_EQ_STR(buffer, "/usr/local");
    TEST_CHECK_EQ_UINT(fs_parent_dir_init("/usr/local/lib", buffer, 4), 11);
}

static void test_move_chmod_remove(void)
{
    build_tree();

    TEST_CHECK(fs_move(path_in_root("a.txt"), path_in_root("moved.txt")));
    TEST_CHECK(!fs_path_exists(path_in_root("a.txt")));
    TEST_CHECK(fs_is_file(path_in_root("moved.txt")));

    TEST_CHECK(fs_move(path_in_root("sub"), path_in_root("sub_moved")));
    TEST_CHECK(fs_is_file(path_in_root("sub_moved/deep/d.txt")));

    TEST_CHECK(fs_chmod(path_in_root("moved.txt"), FsChMod_Rw_Owner));
    TEST_CHECK(fs_chmod(path_in_root("moved.txt"), FsChMod_Rw_All));
    TEST_CHECK(!fs_chmod(path_in_root("nope"), FsChMod_Rw_All));

    TEST_CHECK(fs_remove(path_in_root("moved.txt")));
    TEST_CHECK(!fs_path_exists(path_in_root("moved.txt")));

    TEST_CHECK(fs_remove(g_root));
    TEST_CHECK(!fs_path_exists(g_root));
    TEST_CHECK(!fs_remove(g_root));
}

typedef struct WalkResult {
    char entries[MAX_ENTRIES][MAX_PATH];
    size_t count;
} WalkResult;

static void walk(FSWalkMode mode, WalkResult* result)
{
    FSWalkIterator iterator;
    const size_t root_size = strlen(g_root);

    result->count = 0;

    TEST_ASSERT(fs_walk_iterator_init(&iterator));

    while(fs_walk(g_root, &iterator, mode))
    {
        TEST_ASSERT(result->count < MAX_ENTRIES);
        TEST_ASSERT(strncmp(iterator.current_path, g_root, root_size) == 0);
        {
            char* entry = result->entries[result->count++];
            char* c;

            snprintf(entry, MAX_PATH, "%s", iterator.current_path + root_size + 1);

            for(c = entry; *c != '\0'; c++)
                if(*c == '\\')
                    *c = '/';
        }
    }

    fs_walk_iterator_release(&iterator);
}

static bool walk_contains(const WalkResult* result, const char* entry)
{
    size_t i;

    for(i = 0; i < result->count; i++)
        if(strcmp(result->entries[i], entry) == 0)
            return true;

    return false;
}

static void check_walk(FSWalkMode mode, const char* const* expected, size_t expected_count)
{
    WalkResult result;
    size_t i;

    walk(mode, &result);

    TEST_CHECK_MSG(result.count == expected_count, "mode %d: walked %zu entries, expected %zu", (int)mode, result.count, expected_count);

    for(i = 0; i < expected_count; i++)
        TEST_CHECK_MSG(walk_contains(&result, expected[i]), "mode %d: missing \"%s\"", (int)mode, expected[i]);
}

static void test_walk(void)
{
    static const char* const all[] = {
        "a.txt", "empty_file", "empty_dir", "sub", "sub/c.bin", "sub/deep", "sub/empty_nested", "sub/deep/d.txt"
    };
    static const char* const files_recursive[] = { "a.txt", "empty_file", "sub/c.bin", "sub/deep/d.txt" };
    static const char* const dirs_top[] = { "empty_dir", "sub" };
    static const char* const files_top[] = { "a.txt", "empty_file" };
    FSWalkIterator* iterator;

    build_tree();

    check_walk(FSWalkMode_Recursive | FSWalkMode_YieldFiles | FSWalkMode_YieldDirs, all, 8);
    check_walk(FSWalkMode_Recursive | FSWalkMode_YieldFiles, files_recursive, 4);
    check_walk(FSWalkMode_YieldDirs, dirs_top, 2);
    check_walk(FSWalkMode_YieldFiles, files_top, 2);

    iterator = fs_walk_iterator_new();
    TEST_ASSERT(iterator != NULL);
    TEST_CHECK(!fs_walk(path_in_root("nope"), iterator, FSWalkMode_YieldFiles));
    fs_walk_iterator_free(iterator);

    iterator = fs_walk_iterator_new();
    TEST_CHECK(fs_walk(g_root, iterator, FSWalkMode_YieldFiles));
    fs_walk_iterator_free(iterator);

    fs_remove(g_root);
}

TEST_MAIN(
    TEST(test_file_content),
    TEST(test_queries),
    TEST(test_parent_dir),
    TEST(test_move_chmod_remove),
    TEST(test_walk),
)
