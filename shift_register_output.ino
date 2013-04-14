#define SHIFT_REGISTER DDRB
#define SHIFT_PORT PORTB
#define DATA (1<<PINB3)	      //MOSI (SI)
#define LATCH (1<<PINB2)      //SS   (RCK)
#define CLOCK (1<<PINB5)      //SCK  (SCK)

void init_shift_register(){
  //Setup IO
  SHIFT_REGISTER |= (DATA | LATCH | CLOCK);	//Set control pins as outputs
  SHIFT_PORT &= ~(DATA | LATCH | CLOCK);		//Set control pins low
  //Setup SPI
  SPCR = (1<<SPE) | (1<<MSTR);	//Start SPI as Master
  SPSR = (1<<SPI2X);
}

void spi_send(unsigned char byte){
  SPDR = byte;			//Shift in some data
  while(!(SPSR & (1<<SPIF)));	//Wait for SPI process to finish
}

// h is +5, l is 0v, h -> 0 is conducting, l -> 1 is conducting
void output(unsigned char h, unsigned char l){
  SHIFT_PORT &= ~LATCH;

  spi_send(l);
  spi_send(h);

  //Toggle latch to copy data to the storage register
  SHIFT_PORT |= LATCH;
  SHIFT_PORT &= ~LATCH;
}
