# coding:utf-8

from __future__ import division
import sys, time, telnetlib, ConfigParser, os, exceptions,msvcrt


#
# 程序读取配置文件，返回所有配置文件字段。
#
def readConfig(name):
    config = ConfigParser.ConfigParser()

    try:
        with open(name, 'r') as cfgFile:
            config.readfp(cfgFile)
            return config
    except IOError, e:
        print '程序没有找到配置文件，程序当前目录需要config.ini文件。'
        sys.exit()


# 准备网络
#
# 登陆Telnet，自动输入用户名密码，完成后打印成功
#
def ReadyNet(host, user, passwd):
    try:
        telnetfp = telnetlib.Telnet(host, timeout=3)

        telnetfp.read_until("ogin: ")
        telnetfp.write(user + "\r\n")
        telnetfp.read_until("assword: ")
        telnetfp.write(passwd + "\r\n")
        return telnetfp

    except IOError, e:
        raise e


#
# 获取用户输入产品号，附带输入信息；
#
def getInputGiveInfo():
    try:
        print "请用扫码器扫描22位资产管理编号".decode('utf-8')
        info = ""
        sys.stdout.write("Input ID >>>")
        info = ""
        while len(info) < 22:
            ch = msvcrt.getch()
            if str.isdigit(ch):
                info += ch
            elif ch <> '\r':
                print "输入格式不合格，请重新输入".decode('utf-8')
                sys.stdout.write("Input ID >>>")
                info = ""
        print info
        return info
    except IOError, e:
        print '请输入正确格式的信息。'.decode('utf-8')
    except SyntaxError, e:
        return ''


#
# 检查配置文件是否正确
#
def checkConfig(config):
    config = readConfig(config);
    process = config.options('process')
    for p in process:
        if not config.has_option('parameter', p) or not config.has_option('timer', p):
            print '配置文件参数数量不匹配'
            sys.exit()

    return config


#
# 格式化用户输入ID
#
def formatId(config, deviceId):
    localID = int(deviceId)
    mainID = ""
    while localID != 0:
        part = localID % 100;
        localID //= 100;
        mainID = "%02d" % part + " " + mainID

    return mainID


#
# 处理网络连接，参数配置
#
def doSetDevice(config):
    process = config.options('process')
    nums = len(process)
    bar = "##"
    cur = 0
    for p in process:
        # 滚动进度条
        bar += "##"
        cur += 1
        sys.stdout.write(str(int((cur / nums) * 100)) + '% ||' + bar + '->' + "\r")
        sys.stdout.flush()

        lNet = ReadyNet(config.get('info', 'host'), config.get('info', 'user'), config.get('info', 'passwd'))
        lNet.write(config.get('process', p) + " " + config.get('parameter', p) + "\r\n")
        lNet.write("exit" + "\r\n")
        lNet.read_all()
        time.sleep(config.getint('timer', p))

        lNet.close()


if __name__ == '__main__':
    config = checkConfig("./config.ini")



    while True:
        try:
            #os.system('cls;clear')
            #os.system('arp -d')

            newDeviceId = getInputGiveInfo()[13:-1]
            print newDeviceId
            if newDeviceId is not "":
                deviceId = newDeviceId
            config.set('parameter', 'id', formatId(config, deviceId))

            doSetDevice(config)
            deviceId = "%d" % (int(deviceId) + 1)

        except IOError, e:
            print '网络连接错误，检查网线连接状态。'.decode('utf-8')
            continue
