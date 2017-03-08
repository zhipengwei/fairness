#set -x
#trap read debug
packets_per_flow=20

for numberOfNodes in 2
do
	for lambda in 0.1
	do
		for serverLinkCapacity in 1000000000000
		do
  			for senderDataRate in 160000 # This is the assumption made about the sending rate
			do
				cat $1 > configOnOffApplicationBreakConnection.h
				echo "#define NUMBER_OF_TERMINALS $numberOfNodes" >> configOnOffApplicationBreakConnection.h
				echo "#define EXPERIMENT_CONFIG_SERVER_LINK_DATA_RATE $serverLinkCapacity" >> configOnOffApplicationBreakConnection.h
				echo "#define EXPERIMENT_CONFIG_SENDER_DOWNTIME_MEAN $lambda" >> configOnOffApplicationBreakConnection.h
		        	echo "#define EXPERIMENT_CONFIG_SENDER_PACKETS_PER_SHORT_FLOW $packets_per_flow" >> configOnOffApplicationBreakConnection.h
		        	echo "#define EXPERIMENT_CONFIG_SENDER_DATA_RATE $senderDataRate" >> configOnOffApplicationBreakConnection.h
				echo " " >> onOffApplicationBreakConnection.cc
				cd ..
				./waf 
				echo "Compilation finished!"
				./waf --run onOffApplicationBreakConnection 2>&1 | cat > traces/onOffApplicationBreakConnection_numberOfNodes${numberOfNodes}_Lambda${lambda}_ServerLinkCapacity${serverLinkCapacity}_SenderDataRate${senderDataRate}
				cd scratch
			done
		done
	done
done

