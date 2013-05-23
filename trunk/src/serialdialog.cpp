#include "serialdialog.h"
#include "ui_serialdialog.h"
#include "stamp-eepe.h"
#include "mainwindow.h"
#include <QtGui>
#include "qextserialenumerator.h"
#include <QtCore/QList>
#include <QString>
#include <inttypes.h>

serialDialog::serialDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::serialDialog)
{
  ui->setupUi(this);

	ui->progressBar->hide() ;

	QList<QextPortInfo> ports = QextSerialEnumerator::getPorts() ;
	ui->portCB->clear() ;
  foreach (QextPortInfo info, ports)
	{
  	ui->portCB->addItem(info.portName) ;
	}
	port = NULL ;
}

serialDialog::~serialDialog()
{
	if ( port )
	{
		if (port->isOpen())
		{
    	port->close();
		}
    delete port ;
	}
  delete ui;
}

void serialDialog::on_cancelButton_clicked()
{
	done(0) ;	
}


void serialDialog::on_FileEdit_editingFinished()
{
    fileToSend = ui->FileEdit->text();
}

void serialDialog::on_browseButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("File To Send"),ui->FileEdit->text());

    if(!fileName.isEmpty())
    {
        ui->FileEdit->setText(fileName);
        fileToSend = fileName;
    }
}

void waitMs( int ms )
{
  QTime Time ;
  Time.start() ;

  while( Time.elapsed() < ms )
	{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);    
	}
}

void checksumBlock( unsigned char *block )
{
	uint16_t csum ;
	int i ;

	csum = 0 ;
	block += 3 ;
	for ( i = 0 ; i < 128 ; i += 1 )
	{
		csum += *block++ ;		
	}
	*block++ = csum ;
	*block = csum >> 8 ;	
}

#define BLOCK_AREA		136
#define SOH                     (0x01)  /* start of 128-byte data packet */
#define STX                     (0x02)  /* start of 1024-byte data packet */
#define EOT                     (0x04)  /* end of transmission */
#define ACK                     (0x06)  /* acknowledge */
#define NAK                     (0x15)  /* negative acknowledge */
#define CA                      (0x18)  /* two of these in succession aborts transfer */
#define CRC16                   (0x43)  /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1                  (0x41)  /* 'A' == 0x41, abort by user */
#define ABORT2                  (0x61)  /* 'a' == 0x61, abort by user */

uint8_t DoubleEsc[2] = {27,27} ;
uint8_t DoubleCan[2] = {24,24} ;

QString Monitor ;

int serialDialog::waitForAckNak( int mS )
{
  char buff[150] ;
  int numBytes ;
  QByteArray qba ;
	int j ;
  int i ;
	int timer ;
  char acknak ;

	timer = 0 ;

	do
	{
  	waitMs( 10 ) ;
		
  	numBytes = port->bytesAvailable() ;
		if ( numBytes)
		{
  		if(numBytes > 148)
			{
  		  numBytes = 148 ;
			}

  		i = port->read(buff, numBytes);
  		if (i != -1)
  		  buff[i] = '\0';
  		else
  		  buff[0] = '\0';

  		qba = QByteArray::fromRawData(buff, i ) ;
			acknak = 0 ;
			for ( j = 0 ; j < i ; j += 1 )
			{
  		  if ( qba[j] == (char)NAK )
				{
					acknak = NAK ;
					break ;
				}
  		  if ( qba[j] == (char)ACK )
				{
					acknak = ACK ;
					break ;
				}
			}
			qba = qba.toHex() ;
			Monitor.append(qba);
			ui->monitorText->setText(Monitor) ;
			if ( acknak )
			{
				return acknak ;
			}
		}
//		ui->receiveText->append("!") ;
		timer += 10 ;
	} while ( timer < mS ) ;
  return 0 ;
}

uint8_t NAKcount ;

void serialDialog::on_sendButton_clicked()
{
	// Send the file
  
	QString portname ;
  QString file ;
  QString temp ;
  QByteArray qba ;

	ui->sendButton->setDisabled( true ) ;
	
	ui->progressBar->setValue( 0 ) ;
	ui->progressBar->show() ;
	
	ui->monitorText->setText("Monitor") ;
	Monitor.append("Monitor");

	if(fileToSend.isEmpty())
	{
    QMessageBox::critical(this, "eePe", tr("No files specified"));
		ui->sendButton->setDisabled( false ) ;
		return ;
	}
	
	// Set the serial port
  portname = ui->portCB->currentText() ;
#ifdef Q_OS_UNIX
  port = new QextSerialPort(portname, QextSerialPort::Polling) ;
#else
  port = new QextSerialPort(portname, QextSerialPort::Polling) ;
#endif /*Q_OS_UNIX*/
  port->setBaudRate(BAUD38400) ;
  port->setFlowControl(FLOW_OFF) ;
  port->setParity(PAR_NONE) ;
  port->setDataBits(DATA_8) ;
  port->setStopBits(STOP_1) ;
    //set timeouts to 500 ms
  port->setTimeout(100) ;

	sendOneFile( fileToSend ) ;

	if ( port )
	{
 		port->close();
  	delete port ;
		port = NULL ;
	}

	ui->progressBar->hide() ;
	ui->sendButton->setDisabled( false ) ;
}


int serialDialog::sendOneFile( QString fname )
{
  QString file ;
  QString temp ;
	int blockNumber ;
	uint8_t dataBlock[BLOCK_AREA] ;
	int j ;
  int i ;
  char buff[150] ;
  int numBytes ;
  QByteArray qba ;
	int numBlocks ;
	char acknak ;
	int retryCount ;
	
	if(QFileInfo(fileToSend).exists())
	{
		file = QFileInfo(fileToSend).fileName() ;
    ui->Name->setText( file ) ;
    temp = tr("%1").arg(QFileInfo(fileToSend).size() ) ;
    ui->size->setText( temp ) ;
	}
	else
	{
    QMessageBox::critical(this, "eePe", tr("File not Found"));
		ui->sendButton->setDisabled( false ) ;
    return 0 ;	// Failed
	}
  numBlocks = QFileInfo(fileToSend).size() / 128 + 1 ;
	blockNumber = 0 ;

  memset( dataBlock, 0, BLOCK_AREA ) ;
	dataBlock[0] = SOH ;
	dataBlock[1] = blockNumber ;
	dataBlock[2] = ~blockNumber ;
	strcpy( (char *)&dataBlock[3], file.toLatin1() ) ;
  j = 4 + strlen( file.toLatin1() ) ;
  strcpy( (char *)&dataBlock[j], temp.toLatin1() ) ;
//  j += strlen( temp.toAscii() ) ;
	checksumBlock( dataBlock ) ;

  port->open(QIODevice::ReadWrite | QIODevice::Unbuffered) ;
  
  // Put receiver into file mode
  port->write( QByteArray::fromRawData ( ( char *)DoubleEsc, 2 ), 2 ) ;
  waitMs( 150 ) ;
  numBytes = port->bytesAvailable() ;
  if(numBytes > 148)
	{
    numBytes = 148 ;
	}
  buff[0] = '\0';
  i = port->read(buff, numBytes) ;	// Discard cr,lf,>
  if (i != -1)
	{
    buff[i] = '\0';
	}
//  qba = QByteArray::fromRawData(buff, i ) ;
//  qba = qba.toHex() ;
//	ui->receiveText_2->setText(qba) ;

	retryCount = 10 ;
	do
	{
		i = port->write( QByteArray::fromRawData ( ( char *)dataBlock, 128 + 5 ), 128 + 5 ) ;
//  	temp = tr("%1").arg(i) ;
//  	ui->sizeSent->setText( temp ) ;

  	waitMs( 40 ) ;

		acknak = waitForAckNak( 200 ) ;
		retryCount -= 1 ;

		if ( acknak != ACK )
		{
			NAKcount += 1 ;
//	    temp = tr("%1").arg(NAKcount) ;
//  	  ui->NAKcount->setText( temp ) ;
		}

	} while ( ( acknak != ACK ) && retryCount ) ;
	if ( acknak != ACK )
	{
		// Abort
	 	port->close();
	  delete port ;
		port = NULL ;
    QMessageBox::critical(this, "eePe", tr("Communication Failure(1)"));
		ui->sendButton->setDisabled( false ) ;
		return 0 ;	// Failed
	}
	ui->progressBar->setValue( 100/(numBlocks+1) ) ;
		    
//  QString msg = QLatin1String(buff);
//  temp = tr("%1").arg(i) ;
//  ui->sizeSent->setText( temp ) ;

//  ui->receiveText->setText(acknak ? (acknak == ACK ? "ACK" : "NAK" ) : "NULL") ;

  QFile thisfile(fileToSend) ;
	thisfile.open(QIODevice::ReadOnly) ;
	while ( blockNumber < numBlocks )
	{
		blockNumber += 1 ;
		memset( dataBlock, 0, BLOCK_AREA ) ;
		dataBlock[0] = SOH ;
		dataBlock[1] = blockNumber ;
		dataBlock[2] = ~blockNumber ;
    thisfile.read( (char *)&dataBlock[3], 128 ) ;
		checksumBlock( dataBlock ) ;
		retryCount = 10 ;
		do
		{
			i = port->write( QByteArray::fromRawData ( ( char *)dataBlock, 128 + 5 ), 128 + 5 ) ;
//  		temp = tr("%1").arg(i) ;
//			ui->sizeSent->setText( temp ) ;

//    	qba = QByteArray::fromRawData((char *)dataBlock, 133 ) ;
//  		qba = qba.toHex() ;
//			ui->receiveText_2->setText(tr("Block %1").arg(blockNumber)) ;

			waitMs( 30 ) ;

			acknak = waitForAckNak( 200 ) ;
//  		temp = tr("%1").arg(i) ;
//  		ui->sizeSent->setText( temp ) ;
  		
//    	temp = tr("%1,%2").arg(acknak ? ((acknak == ACK ) ? "ACK" : "NAK" ): "NULL").arg(blockNumber) ;

//    	ui->receiveText->setText(temp) ;
			retryCount -= 1 ;
			if ( acknak != ACK )
			{
				NAKcount += 1 ;
//	  	  temp = tr("%1").arg(NAKcount) ;
//  		  ui->NAKcount->setText( temp ) ;
			}
		} while ( ( acknak != ACK ) && retryCount ) ;
		
		if ( acknak != ACK )
		{
			// Abort
	  	port->write( QByteArray::fromRawData ( ( char *)DoubleEsc, 2 ), 2 ) ;
	 		port->close();
	  	delete port ;
			port = NULL ;
    	QMessageBox::critical(this, "eePe", tr("Communication Failure(1)"));
			ui->sendButton->setDisabled( false ) ;
			return 0 ;	// Failed
		}
	ui->progressBar->setValue( ((blockNumber+1)*100)/(numBlocks+1) ) ;
	}			 
	thisfile.close() ;
	dataBlock[0] = EOT ;
	i = port->write( QByteArray::fromRawData ( ( char *)dataBlock, 1 ), 1 ) ;
//  qba = QByteArray::fromRawData((char *)dataBlock, 1 ) ;
//	qba = qba.toHex() ;
//	ui->receiveText_2->setText(qba) ;
	waitMs( 30 ) ;
	acknak = waitForAckNak( 200 ) ;
//  temp = tr("%1,%2").arg(acknak ? ((acknak == ACK ) ? "ACK" : "NAK" ): "NULL").arg("EOT") ;
//  ui->receiveText->append(temp) ;
		return 1 ;	// Success
}



