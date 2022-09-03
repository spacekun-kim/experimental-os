#include "ne2000.h"
#include "print.h"
#include "io.h"
#include "interrupt.h"
#include "ethernet.h"
#include "queue.h"
#include "ksoftirqd.h"
#include "stdio-kernel.h"

#define MTU_SIZE 1500

#define NE2000_IO_ADDR 0x300 
/* ne2000_registers */
#define CR						NE2000_IO_ADDR + 0x00 // command register
#define RDMA					NE2000_IO_ADDR + 0x10 // remote DMA port
#define RSTPORT				NE2000_IO_ADDR + 0x18 // reset port


/* page 0 */
/* read */
#define CLDA0					NE2000_IO_ADDR + 0x01 // current local DMA address 0
#define CLDA1					NE2000_IO_ADDR + 0x02 // current local DMA address 1
#define BNRY					NE2000_IO_ADDR + 0x03 // boundary pointer (for ring buffer)
#define TSR						NE2000_IO_ADDR + 0x04 // transmit status register
#define NCR						NE2000_IO_ADDR + 0x05 // collisions counter
#define FIFO					NE2000_IO_ADDR + 0x06 // ?
#define ISR						NE2000_IO_ADDR + 0x07 // interrupt status register
#define CRDA0					NE2000_IO_ADDR + 0x08 // current remote DMA address 0
#define CRDA1					NE2000_IO_ADDR + 0x09 // current remote DMA address 1
#define RSR						NE2000_IO_ADDR + 0x0c // receive status register
/* write */
#define PSTART				NE2000_IO_ADDR + 0x01 // page start
#define PSTOP					NE2000_IO_ADDR + 0x02 // page stop
#define TPSR					NE2000_IO_ADDR + 0x04 // transmit page start address
#define TBCR0					NE2000_IO_ADDR + 0x05 // transmit byte count (low)
#define TBCR1					NE2000_IO_ADDR + 0x06 // transmit byte count (high)
#define RSAR0					NE2000_IO_ADDR + 0x08 // remote start address (low)
#define RSAR1					NE2000_IO_ADDR + 0x09 // remote start address (high)
#define RBCR0					NE2000_IO_ADDR + 0x0a // remote byte count (low)
#define RBCR1					NE2000_IO_ADDR + 0x0b // remote byte count (high)
#define RCR						NE2000_IO_ADDR + 0x0c // receive config register
#define TCR						NE2000_IO_ADDR + 0x0d // transmit config register
#define DCR						NE2000_IO_ADDR + 0x0e // data config register
#define IMR						NE2000_IO_ADDR + 0x0f // interrupt mask register

/* page 1 */
#define PAR0					NE2000_IO_ADDR + 0x01 // physical address registers (R/W)
#define PAR1					NE2000_IO_ADDR + 0x02
#define PAR2					NE2000_IO_ADDR + 0x03
#define PAR3					NE2000_IO_ADDR + 0x04
#define PAR4					NE2000_IO_ADDR + 0x05
#define PAR5					NE2000_IO_ADDR + 0x06
#define CURR					NE2000_IO_ADDR + 0x07 // current page register (R/W)
#define MAR0					NE2000_IO_ADDR + 0x08 // multicast address registers (R/W)
#define MAR1					NE2000_IO_ADDR + 0x09
#define MAR2					NE2000_IO_ADDR + 0x0a
#define MAR3					NE2000_IO_ADDR + 0x0b
#define MAR4					NE2000_IO_ADDR + 0x0c
#define MAR5					NE2000_IO_ADDR + 0x0d
#define MAR6					NE2000_IO_ADDR + 0x0e
#define MAR7					NE2000_IO_ADDR + 0x0f

/* command register bits */
#define CR_STOP				0x01 // stop NIC
#define CR_START			0x02 // start NIC
#define CR_TRANSMIT		0x04 // must be 1 to transmit packet
#define CR_DMAREAD		0x08 // remote DMA read
#define CR_DMAWRITE		0x10 // remote DMA write
#define CR_SENDPACKET	0x18 // send packet
#define CR_NODMA			0x20 // abort/complete remote DMA

#define CR_PAGE0			0x00 // select register page 0/1/2
#define CR_PAGE1			0x40
#define CR_PAGE2			0x80

/* ISR register bits */
#define ISR_PRX				0x01 // packet received with no errors
#define ISR_PTX				0x02 // packet transmitted with no error
#define ISR_RXE				0x04 // packet received with one or more of th following errors: CRC error, Frame alignment error, Missed packet
#define ISR_TXE				0x08 // packet transmission is aborted due to excessive collisions
#define ISR_OVW				0x10 // receive buffer has been exhausted
#define ISR_CNT				0x20 // MSB of one or more of the network tally counters has been set
#define ISR_RDC				0x40 // remote DMA operation has been completed
#define ISR_RST				0x80 // NIC enters reset state and is cleared when a start command is issued to the CR

/* Data Control Register */
#define DCR_BYTEDMA		0x00
#define DCR_WORDDMA		0x01
#define DCR_NOLPBK		0x08
#define DCR_ARM				0x10
#define DCR_FIFO2			0x00
#define DCR_FIFO4			0x20
#define DCR_FIFO8			0x40
#define DCR_FIFO12		0x60

/* Receive Control Register */
#define RCR_SEP				0x01
#define RCR_AR				0x02
#define RCR_AB				0x04
#define RCR_AM				0x08
#define RCR_PRO				0x10
#define RCR_MONITOR		0x20

/* Transmission Control Register */
#define TCR_NOLPBK		0x00
#define TCR_INTLPBK		0x02
#define TCR_EXTLPBK		0x04
#define TCR_EXTLPBK2	0x06

#define TXSTART				0x40
#define RXSTART				0x4C
#define RXSTOP				0x80

void intr_network_handler(void) {
	uint16_t isr = inb(ISR);
	// printk("ISR = %x\n", isr);
	// call softioq_thread to recv and solve the packet
	if(isr & 0x01) {
		softirq_queue_put(NET_RX);
	}
	// clear ISR
	outb(ISR, 0xff);
}

void ne2000_init(void) {	
	put_str("network init start.\n");
	/* page 0 */
	outb(CR, CR_PAGE0 | CR_NODMA | CR_STOP);
	outb(DCR, DCR_BYTEDMA | DCR_NOLPBK | DCR_ARM);
	outb(RBCR0, 0x00);
	outb(RBCR1, 0x00);
	outb(RCR, RCR_AM | RCR_AB | RCR_AR);
	outb(TCR, TCR_INTLPBK);

	outb(TPSR, TXSTART); // transmit buffer
	outb(BNRY, RXSTART);
	
	outb(PSTART, RXSTART); // receive buffer
	outb(PSTOP, RXSTOP);

	outb(CR, CR_PAGE0 | CR_NODMA | CR_STOP);
	outb(DCR, DCR_FIFO8 | DCR_NOLPBK | DCR_ARM);
	outb(CR, (CR_NODMA | CR_START));
	outb(ISR, 0xff); // clear all interrupts
	outb(IMR, 0xff); // only recv packets with no error
	outb(TCR, 0x00);

	int i;
	for(i = 0; i < 6; ++i) {
		inb(RDMA);
		mac[5 - i] = inb(RDMA);
	}	
	/* page 1 */
	outb(CR, CR_PAGE1 | CR_NODMA | CR_STOP);
	outb(PAR0, mac[0]);
	outb(PAR1, mac[1]);
	outb(PAR2, mac[2]);
	outb(PAR3, mac[3]);
	outb(PAR4, mac[4]);
	outb(PAR5, mac[5]);
	// idx = mcast_index(reversed_dest_mac) = 100011b
	// !(mchash[idx >> 3] & (1 << (idx & 0x7)))
	// !(mchash[4] & 1000b)
	outb(MAR4, 0x08);

	outb(CURR, RXSTART);
	outb(CR, CR_NODMA | CR_START);

	register_handler(0x2a, intr_network_handler);
	put_str("network init done.\n");
}

/***************************************************** 802.3 protocol *****************************************************/
/* |PReamble|Start-of-frame Delimiter|Destination Address|Source Addresses|Type/Length|Data|Padding|Frame Check Sequence| */
/* |   PR   |           SD           |        DA         |       SA       | TYPE/LEN  |DATA|  PAD  |         FCS        | */
/* |   7    |           1            |        6          |       6        |     2     |  46~1500   |          4         | */
/**************************************************************************************************************************/

/********* send frame ********/
/* |DA|SA|TYPE/LEN|DATA|PAD| */

void ne2000_send(uint8_t* packet, uint16_t length) {
	uint16_t i = 0;
	/* packet must be larger than 0x40 */
	if(length < 0x40) {
		length = 0x40;
	}
	/* if transmitting, wait */
	while(inb(CR) & CR_TRANSMIT);
	/* abort any running DMA operations */
	outb(CR, CR_PAGE0 | CR_NODMA | CR_START);
	/* copy from ram to ne2000 */
	outb(RBCR0, length & 0xff);
	outb(RBCR1, length >> 8);
	outb(RSAR0, 0x00);
	outb(RSAR1, TXSTART);
	outb(CR, CR_PAGE0 | CR_DMAWRITE | CR_START);
	for(i = 0; i < length; ++i) {
		outb(RDMA, packet[i]);
	}
	/* transmit */
	outb(CR, CR_PAGE0 | CR_NODMA | CR_START);
	outb(TBCR0, length & 0xff);
	outb(TBCR1, length >> 8);
	outb(TPSR, TXSTART);
	outb(CR, CR_PAGE0 | CR_START | CR_TRANSMIT);
}

/*************************************** recv frame *************************************/
/* |recv status(1B)|next page pointer(1B)|recv bytes(2B)|DA|SA|TYPE/LEN|DATA|PAD|FCS| */

uint16_t ne2000_recv(uint8_t* packet) {
	uint8_t curr, bnry;
	uint16_t i = 0;
	uint8_t recv_status;
	uint16_t recv_bytes;
	outb(CR, CR_PAGE1 | CR_NODMA | CR_START);
	curr = inb(CURR);
	outb(CR, CR_PAGE0 | CR_NODMA | CR_START);
	bnry = inb(BNRY);

	if(bnry == curr) {
		return 0;
	}
	/* invalid bnry situation: reset buffer */
	if((bnry > RXSTOP) || (bnry < RXSTART)) {
		outb(BNRY, RXSTART);
		outb(CR, (CR_PAGE1 | CR_NODMA | CR_START));
		outb(CURR, RXSTART);
		outb(CR, (CR_NODMA | CR_START));
		return 0;
	}

	if(bnry == RXSTOP) {
		bnry = RXSTART;
	}

	/* read count */
	outb(RBCR0, 0xff);
	outb(RBCR1, 0xff);
	/* read addr */
	outb(RSAR0, 0);
	outb(RSAR1, bnry);
	/* read */
	outb(CR, CR_PAGE0 | CR_DMAREAD | CR_START);
	recv_status = inb(RDMA);
	// to ignore warning
	++recv_status;

	bnry = inb(RDMA);
	if(bnry < RXSTART) {
		bnry = RXSTOP;
	}

	recv_bytes = inb(RDMA);
	recv_bytes += (inb(RDMA) << 8);
	
	if(recv_bytes > MTU_SIZE) {
		recv_bytes = MTU_SIZE;
	}

	for(i = 0; i < recv_bytes; ++i) {
		packet[i] = inb(RDMA);
	}

	outb(CR, CR_PAGE0 | CR_NODMA | CR_START);
	outb(BNRY, bnry);

	return recv_bytes;
}
