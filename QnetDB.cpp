/*
 *   Copyright (C) 2020 by Thomas Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string>
#include <thread>

#include "QnetDB.h"

bool CQnetDB::Open(const char *name)
{
	if (sqlite3_open_v2(name, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL)) {
		fprintf(stderr, "CQnetDB::Open: can't open %s\n", name);
		return true;
	}

	return Init();
}

bool CQnetDB::Init()
{
	char *eMsg;

	std::string sql("CREATE TABLE IF NOT EXISTS LHEARD("
						"callsign	TEXT PRIMARY KEY, "
						"sfx		TEXT, "
						"module		TEXT, "
						"reflector	TEXT, "
						"lasttime	INT NOT NULL"
					") WITHOUT ROWID;");

	if (SQLITE_OK != sqlite3_exec(db, sql.c_str(), NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::Open create table LHEARD error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}

	sql.assign("CREATE TABLE IF NOT EXISTS LINKSTATUS("
					"ip_address		TEXT PRIMARY KEY, "
					"from_mod		TEXT NOT NULL, "
					"to_callsign    TEXT NOT NULL, "
					"to_mod			TEXT NOT NULL, "
					"linked_time	INT NOT NULL"
				") WITHOUT ROWID;");

	if (SQLITE_OK != sqlite3_exec(db, sql.c_str(), NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::Open create table LINKSTATUS error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}

	sql.assign("CREATE TABLE IF NOT EXISTS GATEWAYS("
					"name		TEXT PRIMARY KEY, "
					"address	TEXT NOT NULL, "
					"port		INT NOT NULL"
				") WITHOUT ROWID;");

	if (SQLITE_OK != sqlite3_exec(db, sql.c_str(), NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::Open create table GATEWAYS error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}
	return false;
}

bool CQnetDB::UpdateLH(const char *callsign, const char *sfx, const char module, const char *reflector)
{
	if (NULL == db)
		return false;
	std::string sql("INSERT OR REPLACE INTO LHEARD (callsign,sfx,module,reflector,lasttime) VALUES ('");
	sql.append(callsign);
	sql.append("','");
	sql.append(sfx);
	sql.append("','");
	sql.append(1, module);
	sql.append("','");
	sql.append(reflector);
	sql.append("',");
	sql.append("strftime('%s','now'));");

	char *eMsg;
	if (SQLITE_OK != sqlite3_exec(db, sql.c_str(), NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::UpdateLH error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}

	return false;
}

bool CQnetDB::UpdateLS(const char *address, const char from_mod, const char *to_callsign, const char to_mod, time_t linked_time)
{
	if (NULL == db)
		return false;
	std::string sql = "INSERT OR REPLACE INTO LINKSTATUS (ip_address,from_mod,to_callsign,to_mod,linked_time) VALUES ('";
	sql.append(address);
	sql.append("','");
	sql.append(1, from_mod);
	sql.append("','");
	sql.append(to_callsign);
	sql.append("','");
	sql.append(1, to_mod);
	sql.append("',");
	sql.append(std::to_string(linked_time).c_str());
	sql.append(");");

	char *eMsg;
	if (SQLITE_OK != sqlite3_exec(db, sql.c_str(), NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::UpdateLS error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}

	return false;
}

bool CQnetDB::UpdateGW(const char *name, const char *address, unsigned short port)
{
	if (NULL == db)
		return true;
	std::string n(name);
	n.resize(6, ' ');
	std::string sql = "INSERT OR REPLACE INTO GATEWAYS (name,address,port) VALUES ('";
	sql.append(n);
	sql.append("','");
	sql.append(address);
	sql.append("',");
	sql.append(std::to_string(port));
	sql.append(");");

	char *eMsg;
	if (SQLITE_OK != sqlite3_exec(db, sql.c_str(), NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::UpdateGW error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}

	return false;
}

bool CQnetDB::UpdateGW(CHostQueue &hqueue)
{
	if (NULL == db)
		return false;

	char *eMsg;
	if (SQLITE_OK != sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::UpdateGW BEGIN TRANSATION error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}

	while (! hqueue.Empty()) {
		auto h = hqueue.Pop();
		UpdateGW(h.name.c_str(), h.addr.c_str(), h.port);
	}

	if (SQLITE_OK != sqlite3_exec(db, "COMMIT TRANSACTION;", NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::UpdateGW COMMIT TRANSACTION error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}
	return false;
}

bool CQnetDB::DeleteLS(const char *address)
{
	if (NULL == db)
		return false;
	std::string sql("DELETE FROM LINKSTATUS WHERE ip_address=='");
	sql.append(address);
	sql.append("';");

	char *eMsg;
	if (SQLITE_OK != sqlite3_exec(db, sql.c_str(), NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::DeleteLS error: %s\n", eMsg);
		sqlite3_free(eMsg);
		return true;
	}

	return false;
}

bool CQnetDB::FindLS(const char mod, std::list<CLink> &linklist)
{
	if (NULL == db)
		return false;
	std::string sql("SELECT ip_address,to_callsign,to_mod,linked_time FROM LINKSTATUS WHERE from_mod=='");
	sql.append(1, mod);
	sql.append("';");

	sqlite3_stmt *stmt;
	int rval = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
	if (SQLITE_OK != rval) {
		fprintf(stderr, "CQnetDB::FindLS prepare_v2 error\n");
		return true;
	}

	while (SQLITE_ROW == sqlite3_step(stmt)) {
		std::string cs((const char *)sqlite3_column_text(stmt, 1));
		std::string mod((const char *)sqlite3_column_text(stmt, 2));
		if (mod.at(0) != 'p') {
			cs.resize(7, ' ');
			cs.append(mod);
		}
		linklist.push_back(CLink(cs, sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 3)));
	}

	sqlite3_finalize(stmt);
	return false;
}

bool CQnetDB::FindGW(const char *name, std::string &address, unsigned short &port)
// returns true if NOT found
{
	if (NULL == db)
		return false;
	std::string n(name);
	n.resize(6, ' ');
	std::string sql("SELECT address,port FROM GATEWAYS WHERE name=='");
	sql.append(n);
	sql.append("';");

	sqlite3_stmt *stmt;
	int rval = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
	if (SQLITE_OK != rval) {
		fprintf(stderr, "CQnetDB::FindGW error: %d\n", rval);
		return true;
	}

	if (SQLITE_ROW == sqlite3_step(stmt)) {
		address.assign((const char *)sqlite3_column_text(stmt, 0));
		port = (unsigned short)(sqlite3_column_int(stmt, 1));
		sqlite3_finalize(stmt);
		return false;
	} else {
		sqlite3_finalize(stmt);
		return true;
	}
}

bool CQnetDB::FindGW(const char *name)
// returns true if found
{
	if (NULL == db)
		return false;
	std::string n(name);
	n.resize(6, ' ');
	std::string sql("SELECT address,port FROM GATEWAYS WHERE name=='");
	sql.append(n);
	sql.append("';");

	sqlite3_stmt *stmt;
	int rval = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
	if (SQLITE_OK != rval) {
		fprintf(stderr, "CQnetDB::FindGW error: %d\n", rval);
		return true;
	}

	if (SQLITE_ROW == sqlite3_step(stmt)) {
		sqlite3_finalize(stmt);
		return true;
	} else {
		sqlite3_finalize(stmt);
		return false;
	}
}

void CQnetDB::ClearLH()
{
	if (NULL == db)
		return;

	char *eMsg;

	if (SQLITE_OK != sqlite3_exec(db, "DELETE FROM LHEARD;", NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::ClearLH error: %s\n", eMsg);
		sqlite3_free(eMsg);
	}
}

void CQnetDB::ClearLS()
{
	if (NULL == db)
		return;

	char *eMsg;

	if (SQLITE_OK != sqlite3_exec(db, "DELETE FROM LINKSTATUS;", NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::ClearLS error: %s\n", eMsg);
		sqlite3_free(eMsg);
	}
}

void CQnetDB::ClearGW()
{
	if (NULL == db)
		return;

	char *eMsg;

	if (SQLITE_OK != sqlite3_exec(db, "DELETE FROM GATEWAYS;", NULL, 0, &eMsg)) {
		fprintf(stderr, "CQnetDB::ClearGW error: %s\n", eMsg);
		sqlite3_free(eMsg);
	}
}

static int countcallback(void *count, int /*argc*/, char **argv, char **/*azColName*/) {
    auto c = (int *)count;
    *c = atoi(argv[0]);
    return 0;
}

int CQnetDB::Count(const char *table)
{
	if (NULL == db)
		return 0;

 	std::string sql("SELECT COUNT(*) FROM ");
	sql.append(table);
	sql.append(";");

	int count = 0;

	char *eMsg;
    if (SQLITE_OK != sqlite3_exec(db, sql.c_str(), countcallback, &count, &eMsg)) {
        fprintf(stderr, "CQnetDB::Count error: %s\n", eMsg);
        sqlite3_free(eMsg);
    }

    return count;
}
