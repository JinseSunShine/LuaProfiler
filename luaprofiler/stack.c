/*
** LuaProfiler
** Copyright Kepler Project 2005-2007 (http://www.keplerproject.org/luaprofiler)
** $Id: stack.c,v 1.4 2007-08-22 19:23:53 carregal Exp $
*/

/*****************************************************************************
stack.c:
   Simple stack manipulation
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "stack.h"
#include "clocks.h"
#include "cJSON.h"
//#include <windows.h>

int MAX_CHILD_SIZE = 20;

lprofT_NODE* pTreeRoot = NULL;
lprofT_NODE* pTreeNode = NULL;
lprofT_NODE* pTopRoot = NULL;

char* pOutput = NULL;
int   nMaxStackLevel = 0;
int   nTotalCall = 0;
double dTotalTimeConsuming = 0.0;
int   nOutputCount = 0;
long node_size = 0;
int first_flush = 1;
//time_maker_golbal_begin.QuadPart = 0;
//time_maker_golbal_end.QuadPart = 0;

void output(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vfprintf(outf, format, ap);
	va_end(ap);

	fflush(outf);
}

/* do not allow a string with '\n' and '|' (log file format reserved chars) */
/* - replace them by ' '                                                    */
void formats(char *s) {
	int i;
	if (!s)
		return;
	for (i = strlen(s); i >= 0; i--) {
		if ((s[i] == '|') || (s[i] == '\n'))
			s[i] = ' ';
	}
}

void lprofT_addchild(lprofT_NODE* pParent, lprofT_NODE* pChild)
{
	if (pParent)
	{
		if (pParent->nChildCount >= pParent->nMaxChildCount)
		{
			lprofT_NODE** ppTmp = realloc(pParent->ppChild, pParent->nMaxChildCount * 2 * sizeof(lprofT_NODE));
			assert(ppTmp);
			if (ppTmp)
			{
				pParent->ppChild = ppTmp;
				pParent->ppChild[pParent->nChildCount] = pChild;
				pParent->nMaxChildCount = pParent->nMaxChildCount * 2;
			}
		}
		else
		{
			pParent->ppChild[pParent->nChildCount] = pChild;
		}
		pChild->pParent = pParent;
		pParent->nChildCount++;
		if (pChild && pChild->pNode && pParent->pNode)
			pChild->pNode->interval_time = lprofC_get_seconds2(&pParent->pNode->time_maker_local_time_begin);
	}
}

void lprofT_pop()
{
	if (pTreeNode)
	{
		assert(pTreeNode->pNode);
		pTreeNode->pNode->local_time = lprofC_get_seconds2(&pTreeNode->pNode->time_maker_local_time_begin);
		lprofC_start_timer2(&pTreeNode->pNode->time_maker_local_time_end);
		if (pTreeNode->pNode->stack_level <= 1)
			//dTotalTimeConsuming += pTreeNode->pNode->local_time;
			dTotalTimeConsuming += lprofC_get_interval(&pTreeNode->pNode->time_maker_local_time_begin, &pTreeNode->pNode->time_maker_local_time_end);
		if (pTreeNode->pParent)
		{
			pTreeNode = pTreeNode->pParent;
		}
	}
}

lprofT_NODE* lprofT_createNode(int count)
{
	node_size += count * sizeof(lprofT_NODE);
	lprofT_NODE* pNode = (lprofT_NODE*)malloc(count * sizeof(lprofT_NODE));
	memset(pNode, 0x0, count*sizeof(lprofT_NODE));
	int i = 0;
	for (i = 0; i < count; i++)
	{
		pNode[i].stack_level = 0;
		pNode[i].pNode = NULL;
		pNode[i].ppChild = (lprofT_NODE*)malloc(MAX_CHILD_SIZE*sizeof(lprofT_NODE));
		pNode[i].nChildCount = 0;
		pNode[i].nMaxChildCount = MAX_CHILD_SIZE;
		memset(pNode[i].ppChild, 0x0, MAX_CHILD_SIZE*sizeof(lprofT_NODE));
	}
	
	return pNode;
}


void lprofS_push(lprofS_STACK *p, lprofS_STACK_RECORD r) {
lprofS_STACK q;
        q=(lprofS_STACK)malloc(sizeof(lprofS_STACK_RECORD));
        *q=r;
        q->next=*p;
        *p=q;
		lprofT_add(q);
}

lprofS_STACK_RECORD lprofS_pop(lprofS_STACK *p) 
{
	lprofS_STACK_RECORD r;
	lprofS_STACK q;

        r=**p;
        q=*p;
        *p=(*p)->next;
        //free(q);
		lprofT_pop();
        return r;
}

void lprofT_add(lprofS_STACK pChild)
{
#ifdef _MSC_VER
	if (time_maker_golbal_begin.QuadPart <= 0)
#else
	if (time_maker_golbal_begin.tv_usec <= 0 || time_maker_golbal_begin.tv_sec <= 0)
#endif
	lprofC_start_timer2(&time_maker_golbal_begin);
	
	nTotalCall++;
	lprofT_NODE* p = lprofT_createNode(1);
	p->pNode = pChild;
	p->stack_level = pChild->stack_level;
	lprofC_start_timer2(&(pChild->time_maker_local_time_begin));
	if (pChild->stack_level > nMaxStackLevel)
		nMaxStackLevel = pChild->stack_level;
	if (pTreeRoot == NULL)
	{
		pTreeNode = pTreeRoot = p;
	}
	else
	{
		if (pTreeNode->stack_level == pChild->stack_level)
		{
			lprofT_addchild(pTreeNode->pParent, p);
		}
		else
		{
			lprofT_addchild(pTreeNode, p);
		}

		pTreeNode = p;
	}
	
}


void lprofT_print2()
{
	output("-------------------------------------------------\n");
	pOutput = (char*)malloc((MAX_OUTPUT_MESSAGE_LENGTH + 1)*nMaxStackLevel);
	memset(pOutput, 0x0, (MAX_OUTPUT_MESSAGE_LENGTH + 1)*nMaxStackLevel);
	assert(pOutput);
	lprofT_output(pTreeRoot);
	pTreeNode = pTreeRoot = NULL;
	if (pOutput)
	{
		free(pOutput);
		nOutputCount = 0;
		nMaxStackLevel = 0;
	}
}


void lprofT_print()
{
	if (pTopRoot == NULL)
	{
		pTopRoot = lprofT_createNode(1);
		pTopRoot->stack_level = 0;
	}
	lprofT_addchild(pTopRoot, pTreeNode);
	pTreeNode = pTreeRoot = NULL;
}


void lprofT_output(lprofT_NODE* p)
{
	if (p && p->pNode)
	{
		char* source = p->pNode->source_code;
		if(source == NULL) {
			source = "(string)";
		}
		else {
			formats(source);
		}
		char* name = p->pNode->function_name;

		if (strlen(name) > MAX_FUNCTION_NAME_LENGTH) {
			name = (char*)malloc(MAX_FUNCTION_NAME_LENGTH + 10);
			name[0] = '\"';
			strncpy(name + 1, p->pNode->function_name, MAX_FUNCTION_NAME_LENGTH);
			name[MAX_FUNCTION_NAME_LENGTH] = '"';
			name[MAX_FUNCTION_NAME_LENGTH + 1] = '\0';
		}
		formats(name);
		if (pOutput)
		{
			sprintf(&pOutput[MAX_OUTPUT_MESSAGE_LENGTH*nOutputCount], "%d\t%s\t%s\t%d\t%d\t%f\t\n", p->pNode->stack_level, source, name,
				p->pNode->line_defined, p->pNode->current_line, p->pNode->local_time);
			nOutputCount++;
		}
		int i = 0;
		for (i = 0; i < p->nChildCount; i++)
			lprofT_output(p->ppChild[i]);
		if (pOutput)
		{
			if (p->nChildCount <= 0)
			{
				int i = 0;
				for (i = 0; i < nOutputCount*MAX_OUTPUT_MESSAGE_LENGTH;i += MAX_OUTPUT_MESSAGE_LENGTH)
				{
					char tmp[MAX_OUTPUT_MESSAGE_LENGTH];
					memset(tmp, 0x0, MAX_OUTPUT_MESSAGE_LENGTH);
					#ifdef _MSC_VER
					strcpy_s(tmp, MAX_OUTPUT_MESSAGE_LENGTH, &pOutput[i]);
					#else
					memcpy(tmp, &pOutput[i], (size_t)MAX_OUTPUT_MESSAGE_LENGTH);
					#endif
					
					//printf("%s\n", tmp);
					output("%s\n", tmp);
				}
				output("----\n");
			}
			nOutputCount--;
		}
		lprofT_free(p);
	}
}


void lprofT_free(lprofT_NODE* p)
{
	if (p)
	{
		if (p->pNode)
		{
			free(p->pNode);
			p->pNode = NULL;
		}
		free(p->ppChild);
		free(p);
		p = NULL;
	}
	
}

cJSON* treeTojson2(lprofT_NODE* p)
{
	assert(p);
	cJSON* root = NULL;
	if (p && p->pNode)
	{
		root = cJSON_CreateObject();
		cJSON_AddItemToObject(root, "ln", cJSON_CreateNumber(p->pNode->current_line));
		//cJSON_AddItemToObject(root, "lnDef", cJSON_CreateNumber(p->pNode->line_defined));
		cJSON_AddItemToObject(root, "cs", cJSON_CreateNumber(p->pNode->local_time)); // time consuming
		cJSON_AddItemToObject(root, "lv", cJSON_CreateNumber(p->pNode->stack_level));
		//cJSON_AddItemToObject(root, "itv", cJSON_CreateNumber(p->pNode->interval_time));
		cJSON_AddItemToObject(root, "info", cJSON_CreateString(p->pNode->what));

		char* source = p->pNode->file_defined;
		if (source == NULL || strcmp(source, "") == 0) {
			source = "(string)";
		}
		cJSON_AddItemToObject(root, "mod", cJSON_CreateString(source)); // module name
		char* name = p->pNode->function_name;
		formats(name);
		cJSON_AddItemToObject(root, "fn", cJSON_CreateString(name));	// function name
		{
			cJSON* pChild = cJSON_CreateArray();
			if (pChild)
				cJSON_AddItemToObject(root, "sub", pChild);
			int i = 0;
			for (i = 0; i < p->nChildCount; i++)
				cJSON_AddItemToArray(pChild, treeTojson2(p->ppChild[i]));
		}

		lprofT_free(p);
	}
	return root;
}

void lprofT_tojson2()
{
	if (pTreeRoot)
	{
		if(first_flush)
			output("[");
		first_flush = 0;

		cJSON* root = treeTojson2(pTreeRoot);
		char *jstring = cJSON_Print(root);
		output(jstring);
		cJSON_Delete(root);
		free(jstring);
		pTreeRoot = NULL;
		node_size = 0;

		output(",");
	}
}

void lprofT_close()
{
	cJSON* root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "total_cs", cJSON_CreateNumber(dTotalTimeConsuming));
	cJSON_AddItemToObject(root, "total_call", cJSON_CreateNumber(nTotalCall));
	char *jstring = cJSON_Print(root);
	output(jstring);
	cJSON_Delete(root);
	free(jstring);
	nTotalCall = 0;
	dTotalTimeConsuming = 0.0;
	first_flush = 1;
	output("]");
}

cJSON* treeTojson(lprofT_NODE* p)
{
	assert(p);
	cJSON* root = NULL;
	if (p && p->pNode)
	{
		double beginTime = lprofC_get_interval(&time_maker_golbal_begin, &p->pNode->time_maker_local_time_begin);
		double endTime = lprofC_get_interval(&time_maker_golbal_begin, &p->pNode->time_maker_local_time_end);
		double consumingTimer = lprofC_get_interval(&p->pNode->time_maker_local_time_begin, &p->pNode->time_maker_local_time_end);
		root = cJSON_CreateObject();
		cJSON_AddItemToObject(root, "currentLine", cJSON_CreateNumber(p->pNode->current_line));
		cJSON_AddItemToObject(root, "lineDefined", cJSON_CreateNumber(p->pNode->line_defined));
		cJSON_AddItemToObject(root, "timeConsuming", cJSON_CreateNumber(p->pNode->local_time));
		cJSON_AddItemToObject(root, "stackLevel", cJSON_CreateNumber(p->pNode->stack_level));
		cJSON_AddItemToObject(root, "interval", cJSON_CreateNumber(p->pNode->interval_time));
		cJSON_AddItemToObject(root, "callType", cJSON_CreateString(p->pNode->what));
		cJSON_AddItemToObject(root, "begintime", cJSON_CreateNumber(beginTime));
		cJSON_AddItemToObject(root, "endtime", cJSON_CreateNumber(endTime));
		char* source = p->pNode->file_defined;
		if (source == NULL || strcmp(source, "") == 0) {
			source = "(string)";
		}
		cJSON_AddItemToObject(root, "moduleName", cJSON_CreateString(source));
		char* name = p->pNode->function_name;
		formats(name);
		cJSON_AddItemToObject(root, "funcName", cJSON_CreateString(name));
		{
			cJSON* pChild = cJSON_CreateArray();
			if (pChild)
				cJSON_AddItemToObject(root, "children", pChild);
			int i = 0;
			for (i = 0; i < p->nChildCount; i++)
				cJSON_AddItemToArray(pChild, treeTojson(p->ppChild[i]));
		}

		lprofT_free(p);
	}
	else if (p->stack_level == 0)
	{
		double begin = lprofC_get_millisecond(&time_maker_golbal_begin);
		lprofC_start_timer2(&time_maker_golbal_end);
		double end = lprofC_get_millisecond(&time_maker_golbal_end);
		root = cJSON_CreateObject();
		cJSON*top = cJSON_CreateObject();
		cJSON*zore = cJSON_CreateObject();
		cJSON_AddItemToObject(root, "c", top);
		cJSON_AddItemToObject(top, "objectName", cJSON_CreateString("c lua profiler"));
		cJSON_AddItemToObject(top, "timeConsuming", cJSON_CreateNumber(dTotalTimeConsuming));
		cJSON_AddItemToObject(top, "programName", cJSON_CreateString("lua profiler"));
		cJSON_AddItemToObject(top, "totalCalls", cJSON_CreateNumber(nTotalCall));
		cJSON_AddItemToObject(top, "callStats", zore);
		cJSON_AddItemToObject(zore, "stackLevel", cJSON_CreateNumber(0));
		cJSON_AddItemToObject(zore, "currentLine", cJSON_CreateNumber(0));
		cJSON_AddItemToObject(zore, "lineDefined", cJSON_CreateNumber(0));
		cJSON_AddItemToObject(zore, "moduleName", cJSON_CreateString("lua zore"));
		cJSON_AddItemToObject(zore, "funcName", cJSON_CreateString("c call"));
		cJSON_AddItemToObject(zore, "timeConsuming", cJSON_CreateNumber(dTotalTimeConsuming));
		cJSON_AddItemToObject(zore, "callType", cJSON_CreateString("C"));
		cJSON_AddItemToObject(zore, "interval", cJSON_CreateNumber(0));
		cJSON_AddItemToObject(zore, "begintime", cJSON_CreateNumber(begin));
		cJSON_AddItemToObject(zore, "endtime", cJSON_CreateNumber(end));


		cJSON* pChild = cJSON_CreateArray();
		if (pChild)
			cJSON_AddItemToObject(zore, "children", pChild);
		int i = 0;
		for (i = 0; i < p->nChildCount; i++)
			cJSON_AddItemToObject(pChild, "callStats", treeTojson(p->ppChild[i]));
	}
	return root;
}

/*
	写文件多线程版本，暂时不使用
	2016-08-10 lennon.c
*/
void thread_func()
{
	lprofT_tojson();

	if (outf)
		fclose(outf);
}

#ifdef _MSC_VER
unsigned int __stdcall thread_func_win(void* m)
#else
unsigned int thread_func_win(void* m)
#endif
{
	thread_func();
	return 0;
}

void* thread_func_linux(void* m)
{
	thread_func();
#ifndef _MSC_VER
	pthread_exit(NULL);
#endif
	return NULL;
}

void lprofT_tojson()
{	
	clock_t st, ed;
	st = clock();
	cJSON* root = treeTojson(pTopRoot);
	ed = clock();
	//output("\nJson:%d", (ed - st) / CLK_TCK);
	char *jstring = cJSON_Print(root);	

	st = clock();
	output(jstring);
	ed = clock();
	//output("\nFile:%d", (ed - st) / CLK_TCK);

	cJSON_Delete(root);	
	free(jstring);
	//output("\nMax Size:%d", node_size);
	nTotalCall = 0;
	dTotalTimeConsuming = 0.0;
#ifdef _MSC_VER
	time_maker_golbal_begin.QuadPart = 0;
	time_maker_golbal_end.QuadPart = 0;
#else
	time_maker_golbal_begin.tv_sec = 0;
	time_maker_golbal_begin.tv_usec = 0;

	time_maker_golbal_end.tv_sec = 0;
	time_maker_golbal_end.tv_usec = 0;
#endif	
}

void lprofT_tojson_thread()
{
#ifdef _MSC_VER
	_beginthreadex(NULL, 0, thread_func_win, NULL, 0, NULL);
#else
	pthread_t thr;
	pthread_create(&thr, NULL, thread_func_linux, NULL);
#endif
}
/*
	½áÊø
*/
