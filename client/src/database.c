#include "database.h"

int init_db(sqlite3 **db)
{
	int rc = sqlite3_open("data.db", db);
	if (rc != SQLITE_OK)
	{
		perror("sqlite3_open failed");
		sqlite3_close(*db);
		*db = NULL;
		error_exit("sqlite3_open", *db);
	}

	const char *sql_create_table =
		"CREATE TABLE IF NOT EXISTS pending_data ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT, "
		"message TEXT NOT NULL, "
		"timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";

	char *err_msg = 0;
	rc = sqlite3_exec(*db, sql_create_table, 0, 0, &err_msg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "Failed to create table: %s\n", err_msg);
		sqlite3_free(err_msg);
		error_exit("sqlite3_exec create table", *db);
	}
	printf("Database initialized and table 'pending_data' is ready.\n");
}

void store_data_in_db(const char *message, sqlite3 *db)
{
	if (!db)
	{
		printf("Database not initialized. Cannot store data.\n");
		return;
	}
	sqlite3_stmt *stmt;
	const char *sql_insert = "INSERT INTO pending_data (message) VALUES (?);";
	int rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
	if (rc != SQLITE_OK)
	{
		printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
		return;
	}
	sqlite3_bind_text(stmt, 1, message, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		fprintf(stderr, "Failed to execute statement: %s\\n", sqlite3_errmsg(db));
	}
	else
	{
		printf("Data stored in DB: %s\\n", message);
	}
	sqlite3_finalize(stmt);
}

int is_dbdata(sqlite3 *db)
{
	if (!db)
	{
		fprintf(stderr, "Database not initialized. Cannot check for data.\\n");
		return 0;
	}
	sqlite3_stmt *stmt;
	const char *sql_count = "SELECT COUNT(*) FROM pending_data;";
	int count = 0;
	int rc = sqlite3_prepare_v2(db, sql_count, -1, &stmt, 0);
	if (rc == SQLITE_OK)
	{
		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			count = sqlite3_column_int(stmt, 0);
		}
	}
	else
	{
		fprintf(stderr, "Failed to prepare count statement: %s\\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);
	return count > 0;
}

void send_dbdata(int skfd, sqlite3 *db)
{
	// 检查数据库是否已初始化
	if (!db)
	{
		fprintf(stderr, "Database not initialized. Cannot send data.\n"); // 数据库未初始化。无法发送数据。
		return;
	}

	sqlite3_stmt *stmt_select = NULL;
	// SQL语句：从pending_data表中选择id和message，按id升序排列，只取第一条记录
	const char *sql_select = "SELECT id, message FROM pending_data ORDER BY id ASC LIMIT 1;";

	// 准备select语句
	if (sqlite3_prepare_v2(db, sql_select, -1, &stmt_select, 0) != SQLITE_OK)
	{
		fprintf(stderr, "Failed to prepare select statement: %s\n", sqlite3_errmsg(db)); // 准备select语句失败
		sqlite3_finalize(stmt_select);													 // 如果准备失败，stmt_select可能为NULL，但finalize是安全的
		return;
	}

	// 获取一行数据
	int step_rc = sqlite3_step(stmt_select);
	if (step_rc == SQLITE_ROW)
	{																			 // 如果成功获取到一行数据
		long data_id = sqlite3_column_int(stmt_select, 0);						 // 获取id列的值
		const unsigned char *message_text = sqlite3_column_text(stmt_select, 1); // 获取message列的值

		if (message_text)
		{																	  // 仅当消息不为NULL时继续
			printf("Sending DB data (ID: %ld): %s\n", data_id, message_text); // 正在发送数据库数据
			size_t message_len = strlen((const char *)message_text);		  // 计算消息长度
			ssize_t written_bytes = write(skfd, message_text, message_len);	  // 通过socket发送数据

			if (written_bytes == -1)
			{										 // 发送失败
				perror("send_dbdata: write failed"); // send_dbdata:写入失败
			}
			else if (written_bytes < (ssize_t)message_len)
			{ // 部分发送
				fprintf(stderr, "send_dbdata: partial write, sent %zd of %zu bytes for ID %ld\n",
						written_bytes, message_len, data_id); // send_dbdata: 部分写入，为ID %ld 发送了 %zu 字节中的 %zd 字节
			}
			else
			{
				// 数据发送成功，现在删除它
				printf("DB data (ID: %ld) sent successfully. Deleting from DB.\n", data_id); // 数据库数据 (ID: %ld) 发送成功。正在从数据库删除。

				sqlite3_stmt *stmt_delete = NULL;
				// SQL语句：从pending_data表中删除指定id的记录
				const char *sql_delete = "DELETE FROM pending_data WHERE id = ?;";

				// 准备delete语句
				if (sqlite3_prepare_v2(db, sql_delete, -1, &stmt_delete, 0) == SQLITE_OK)
				{
					sqlite3_bind_int(stmt_delete, 1, data_id); // 绑定id参数
					if (sqlite3_step(stmt_delete) != SQLITE_DONE)
					{																							 // 执行删除操作
						fprintf(stderr, "Failed to delete record (ID: %ld): %s\n", data_id, sqlite3_errmsg(db)); // 删除记录 (ID: %ld) 失败
					}
					else
					{
						printf("Record (ID: %ld) deleted from DB.\n", data_id); // 记录 (ID: %ld) 已从数据库删除。
					}
				}
				else
				{
					fprintf(stderr, "Failed to prepare delete statement for ID %ld: %s\n", data_id, sqlite3_errmsg(db)); // 为ID %ld 准备删除语句失败
				}
				sqlite3_finalize(stmt_delete); // 释放delete语句（如果为NULL也安全）
			}
		}
		else
		{
			// 这种情况在有 "TEXT NOT NULL" 约束的情况下应该很少见。
			// 如果发生，可能表示SQLite内部问题（例如，内存不足）。
			fprintf(stderr, "send_dbdata: Fetched NULL message for ID %ld (unexpected).\n", data_id); // send_dbdata: 为ID %ld 获取到NULL消息（意外）。
		}
	}
	else if (step_rc != SQLITE_DONE)
	{
		// SQLITE_DONE 表示没有找到行，这对于此函数的目的而言不是错误。
		// sqlite3_step 的任何其他结果代码都表示错误。
		fprintf(stderr, "Failed to retrieve data from DB: %s\n", sqlite3_errmsg(db)); // 从数据库检索数据失败
	}
	// 如果 step_rc == SQLITE_DONE，表示没有待处理数据。这是正常的，所以不打印消息。

	sqlite3_finalize(stmt_select); // 释放select语句
}
