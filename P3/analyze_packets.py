import nest_asyncio
nest_asyncio.apply()
from scapy.all import *
from statistics import mean
import sys
from datetime import datetime

REPLY_TYPE = 0
REQUEST_TYPE = 8

def get_pcap_file():
    assert len(sys.argv) == 2, "usage: python3 analyze_packets.py filename.pcap"
    return sys.argv[1]

def get_ICMP_packets(pcap_file):
    # Reads PCAP file and gets list of all packets
    packets = rdpcap(pcap_file)
    # Filter the list of packets to get only ICMP packets
    packet_list = [pkt for pkt in packets if ICMP in pkt]

    return packet_list

def get_source_destination_ips(packets):
    source_ip, destination_ip = None, None
    for pkt in packets:
        # Try to get source and destination IP from a request packet
        if pkt[ICMP].type == REQUEST_TYPE:
            source_ip = pkt.sprintf("%IP.src%")
            destination_ip = pkt.sprintf("%IP.dst%")

        # Try to get the source and destination IP from a reply packet
        elif pkt[ICMP].type == REPLY_TYPE:
            destination_ip = pkt.sprintf("%IP.src%")
            source_ip = pkt.sprintf("%IP.dst%")
        
        # As soon as both the source and destination IPs are defined,
        # return them
        if source_ip is not None and destination_ip is not None:
            return source_ip, destination_ip
    
    return source_ip, destination_ip
        

def get_mean_interval(timestamps):
    timestamps.sort()
    prev_time = timestamps[0]
    intervals = []
    for time in timestamps[1:]:
        intervals.append(time - prev_time)
        prev_time = time
    return mean(intervals)

def calculate_metrics(packet_list):
    
    source_ip, destination_ip = get_source_destination_ips(packets)
    print(f"Source IP: {source_ip}")
    print(f"Destination IP: {destination_ip}\n")
    
    # Filter packets to only get the ones going from source to destination
    # (excluding replies)
    incoming_packets = [pkt for pkt in packet_list if pkt.sprintf("%IP.src%") == source_ip]
    
    timestamps = [pkt.time for pkt in incoming_packets]

    print("transmission started at",datetime.fromtimestamp(int(min((timestamps)))))
    print("transmission endend at",datetime.fromtimestamp(int(max(timestamps))))
    print()
    print(f"Total of ICMP packets exchanged (requests + replies): {len(packet_list)}")
    print(f"Total of ICMP packets sent from source to destination: {len(incoming_packets)}\n")

    mean_interval = get_mean_interval(timestamps)
    print(f"Mean interval between packets: {mean_interval:.2f}")

    throughput = len(incoming_packets)/(max(timestamps) - min(timestamps))
    print(f"Mean Throughput = {throughput:.3f} packets per second.\n")
    
    return packets

if __name__ == "__main__":
    pcap_file = get_pcap_file()

    packets = get_ICMP_packets(pcap_file)
    calculate_metrics(packets)
