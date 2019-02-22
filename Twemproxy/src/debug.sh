# ./nutcracker -c /opt/tiger/kv/twemproxy/conf/nutcracker_test.yml -v 11 -M /opt/tiger/kv/twemproxy/test/manage.sock -o /opt/tiger/kv/twemproxy/test/manage.log
pid=`ps -fe | grep tw| grep -v grep| awk '{print $2}'`
gdb nutcracker $pid
