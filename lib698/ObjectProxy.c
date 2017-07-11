/*
 * ObjectProxy.c
 *
 *  Created on: Nov 12, 2016
 *      Author: ava
 */

#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "AccessFun.h"
#include "StdDataType.h"
#include "Objectdef.h"
#include "PublicFunction.h"
#include "dlt698.h"
#include "dlt698def.h"
#include "../libMq/libmmq.h"

extern ProgramInfo *memp;
extern void getoad(INT8U *data,OAD *oad);
extern int FrameHead(CSINFO *csinfo,INT8U *buf);
extern void FrameTail(INT8U *buf,int index,int hcsi);

void ProxyListResponse(PROXY_GETLIST *list,CommBlock *com)
{
	if (com==NULL || list==NULL)
		return;
	CSINFO csinfo;
	INT8U *sendbuf = com->SendBuf;
	int index=0, hcsi=0,datalen=0 ,apduplace =0;

	memcpy(&csinfo,&list->csinfo,sizeof(CSINFO));
	csinfo.dir = 1;
	csinfo.prm = 1;

	index = FrameHead(&csinfo,sendbuf);
	hcsi = index;
	index = index + 2;

	apduplace = index;		//记录APDU 起始位置
	sendbuf[index++] = PROXY_RESPONSE;
	sendbuf[index++] = list->proxytype;
	sendbuf[index++] = list->piid;
	datalen = list->datalen ;
	if (datalen > 512)
		datalen =512;
	memcpy(&sendbuf[index],list->data,datalen);
	index = index + datalen;
	sendbuf[index++] = 0;
	sendbuf[index++] = 0;
	FrameTail(sendbuf,index,hcsi);
	if(com->p_send!=NULL)
		com->p_send(com->phy_connect_fd,sendbuf,index+3);
}

int getProxylist(INT8U *data,PROXY_GETLIST *getlist)
{
	int i=0,k=0, iindex=0;
	INT8U num=0,oadnum=0;
	INT16U timeout=0;
	OAD oadtmp;
	getlist->num = data[iindex++];// sequence of 代理
	fprintf(stderr,"\n---%d",getlist->num);
	for(i=0;i<getlist->num;i++)
	{
		num = data[iindex];
		if (num>sizeof(getlist->proxy_obj.objs[i].tsa))
			num = sizeof(getlist->proxy_obj.objs[i].tsa);
		memcpy(&getlist->proxy_obj.objs[i].tsa,&data[iindex],num+1);
		iindex = iindex + num +1;
		timeout = data[iindex];
		getlist->proxy_obj.objs[i].onetimeout = timeout<<8 |data[iindex+1];
		iindex = iindex + 2;
		oadnum = data[iindex++];
		getlist->proxy_obj.objs[i].num = oadnum;
		for(k=0; k<oadnum; k++)
		{
			getOAD(0,&data[iindex],&oadtmp,NULL);
			memcpy(&getlist->proxy_obj.objs[i].oads[k],&oadtmp,sizeof(oadtmp));
			iindex = iindex + 4;
		}
	}
	return iindex;
}
int Proxy_GetRequestlist(INT8U *data,CSINFO *csinfo,INT8U *sendbuf,INT8U piid)
{
	INT16U timeout=0 ;
	int i=0,j=0;
	PROXY_GETLIST getlist;
	INT8S	ret=0;

	timeout = data[0] ;
	timeout = timeout <<8 | data[1];

	getlist.timeout = timeout;
	getlist.piid = piid;
	getlist.proxytype = ProxyGetRequestList;
	getProxylist(&data[2],&getlist);
	fprintf(stderr,"\nProxy_GetRequestlist, timeout =%d  代理的对象属性读取数量 %d",timeout,getlist.num);
	for(i=0;i<getlist.num;i++)
	{
		if (getlist.num>10) break;
		fprintf(stderr,"\n第%d组代理对象",i);
		for(j=0; j<getlist.proxy_obj.objs[i].num; j++)
		{
			fprintf(stderr,"\n%04x %02x %02x",getlist.proxy_obj.objs[i].oads[j].OI,getlist.proxy_obj.objs[i].oads[j].attflg,getlist.proxy_obj.objs[i].oads[j].attrindex);
		}
	}
	//写入文件，等待转发			规约中只负责解析代理的内容，并追加写入到代理文件 /nand/proxy_list
	getlist.timeold = time(NULL);
	memcpy(&getlist.csinfo,csinfo,sizeof(CSINFO));

	ret= mqs_send((INT8S *)PROXY_485_MQ_NAME,1,ProxyGetResponseList,(INT8U *)&getlist,sizeof(PROXY_GETLIST));
	fprintf(stderr,"\n代理消息已经发出,ret=%d ,getlist_len=%d\n\n",ret,sizeof(PROXY_GETLIST));
	return 1;
}
int Proxy_GetRequestRecord(INT8U *data,CSINFO *csinfo,INT8U *sendbuf,INT8U piid)
{
	INT8U num=0;
	INT16U timeout=0 ;
	int iindex=0,rsdlen=0;
	PROXY_GETLIST getlist;
	INT8S	ret=0;
	OAD oadtmp;
	RESULT_RECORD record={};

	timeout = data[0] ;
	timeout = timeout <<8 | data[1];

	getlist.timeout = timeout;
	getlist.piid = piid;
	getlist.proxytype = ProxyGetRequestRecord;
	iindex = 2;
	num = data[iindex];
	if (num>sizeof(TSA))
		num = sizeof(TSA);
	memcpy(&getlist.proxy_obj.record.tsa,&data[iindex],num+1);
	iindex = iindex + num +1;
	getOAD(0,&data[iindex],&oadtmp,NULL);
	memcpy(&getlist.proxy_obj.record.oad,&oadtmp,sizeof(oadtmp));
	iindex = iindex + 4;

	rsdlen = get_BasicRSD(0,&data[iindex],(INT8U *)&record.select,&record.selectType);
	getlist.proxy_obj.record.selectbuf.len  = rsdlen;
	getlist.proxy_obj.record.selectbuf.type = record.selectType;
	if (rsdlen<=512)
		memcpy(getlist.proxy_obj.record.selectbuf.buf,&record.select,rsdlen);
	iindex  += rsdlen;
	iindex  += get_BasicRCSD(0,&data[iindex],&getlist.proxy_obj.record.rcsd.csds);

	//写入文件，等待转发			规约中只负责解析代理的内容，并追加写入到代理文件 /nand/proxy_list
	getlist.timeold = time(NULL);
	memcpy(&getlist.csinfo,csinfo,sizeof(CSINFO));

	ret= mqs_send((INT8S *)PROXY_485_MQ_NAME,1,ProxyGetResponseList,(INT8U *)&getlist,sizeof(PROXY_GETLIST));
	fprintf(stderr,"\n代理消息已经发出,ret=%d\n\n",ret);
	return 1;
}

void printcmd(PROXY_GETLIST getlist)
{
	int i=0;
	fprintf(stderr,"proxytype = %d\n",getlist.proxytype);
	fprintf(stderr,"OAD=%04x-%02x-%02x\n",getlist.proxy_obj.transcmd.oad.OI,getlist.proxy_obj.transcmd.oad.attflg,getlist.proxy_obj.transcmd.oad.attrindex);
	fprintf(stderr,"COMDCB:baud=%d,par=%d,datab=%d,stopb=%d,flow=%d\n",getlist.proxy_obj.transcmd.comdcb.baud,getlist.proxy_obj.transcmd.comdcb.verify,
			getlist.proxy_obj.transcmd.comdcb.databits,getlist.proxy_obj.transcmd.comdcb.stopbits,getlist.proxy_obj.transcmd.comdcb.flow);
	fprintf(stderr,"RevTimeOut=%d,ByteTimeOut=%d\n",getlist.proxy_obj.transcmd.revtimeout,getlist.proxy_obj.transcmd.bytetimeout);
	fprintf(stderr,"autoCmdLen=%d\n",getlist.proxy_obj.transcmd.cmdlen);
	for(i=0;i<getlist.proxy_obj.transcmd.cmdlen;i++) {
		fprintf(stderr,"%02x ",getlist.proxy_obj.transcmd.cmdbuf[i]);
	}
}

int Proxy_TransCommandRequest(INT8U *data,CSINFO *csinfo,INT8U *sendbuf,INT8U piid)
{
	PROXY_GETLIST getlist;
	INT8S	ret=0;
	INT16U	index=0;
	INT8U	DAR=success;
	getlist.piid = piid;
	getlist.proxytype = ProxyTransCommandRequest;
	index += getOAD(0,&data[index],&getlist.proxy_obj.transcmd.oad,NULL);
	index += getCOMDCB(0,&data[index],&getlist.proxy_obj.transcmd.comdcb,&DAR);
	getlist.proxy_obj.transcmd.revtimeout = (data[index]<<8) | data[index+1];
	index += 2;
	getlist.proxy_obj.transcmd.bytetimeout = (data[index]<<8) | data[index+1];
	index += 2;
	getlist.proxy_obj.transcmd.cmdlen = data[index++];		//默认长度不超过255
	memcpy(getlist.proxy_obj.transcmd.cmdbuf,&data[index],getlist.proxy_obj.transcmd.cmdlen);
	//写入文件，等待转发			规约中只负责解析代理的内容，并追加写入到代理文件 /nand/proxy_list
	getlist.timeold = time(NULL);
	memcpy(&getlist.csinfo,csinfo,sizeof(CSINFO));

	printcmd(getlist);
	fprintf(stderr,"sizeof(getlist)=%d\n",sizeof(PROXY_GETLIST));

	ret= mqs_send((INT8S *)PROXY_485_MQ_NAME,1,ProxyGetResponseList,(INT8U *)&getlist,sizeof(PROXY_GETLIST));
	fprintf(stderr,"\n代理消息已经发出,ret=%d\n\n",ret);
	return 1;
}
