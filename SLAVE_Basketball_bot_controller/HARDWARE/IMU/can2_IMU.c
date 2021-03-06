#include "can2_IMU.h"
#include "led.h"
#include "delay.h"
#include "string.h"
//////////////////////////////////////////////////////////////////////////////////	 
//XM STM32F407主控板
//CAN驱动代码	   
//XM 电子组
//创建日期:2015/8/30
//版本：V1.0
//西北工业大学舞蹈机器人基地 家政								  
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
//CAN初始化                                                                                                             //
//tsjw:重新同步跳跃时间单元. @ref CAN_synchronisation_jump_width   范围: ; CAN_SJW_1tq~ CAN_SJW_4tq                     //
//tbs2:时间段2的时间单元.   @ref CAN_time_quantum_in_bit_segment_2 范围:CAN_BS2_1tq~CAN_BS2_8tq;                        //
//tbs1:时间段1的时间单元.   @refCAN_time_quantum_in_bit_segment_1  范围: ;	  CAN_BS1_1tq ~CAN_BS1_16tq                 //
//brp :波特率分频器.范围:1~1024;(实际要加1,也就是1~1024) tq=(brp)*tpclk1                                                //
//波特率=Fpclk1/((tsjw+tbs1+tbs2+3)*brp);                                                                               //
//mode: @ref CAN_operating_mode 范围：CAN_Mode_Normal,普通模式;CAN_Mode_LoopBack,回环模式;                              //
//Fpclk1的时钟在初始化的时候设置为36M,如果设置CAN_Normal_Init(CAN_SJW_1tq,CAN_BS2_6tq,CAN_BS1_7tq,6,CAN_Mode_LoopBack); //
//则波特率为:42M/((1+6+7)*6)=500Kbps                                                                                    //
//返回值:0,初始化OK;                                                                                                    //
//    其他,初始化失败;                                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CanRxMsg RxMessage;//定义全局变量存储CAN收到的数据，并在main.c中使用

__IO u8 Flag1_CAN_Receive=0xff, Flag2_CAN_Receive=0xff; //CAN中断接收标志位，成功是标志位为0
__IO u8 GYPOS_DATA[4]={0};   //存储陀螺仪传回的数据

u8 CAN2_Mode_Init(u8 tsjw,u8 tbs2,u8 tbs1,u16 brp,u8 mode)
{

  	GPIO_InitTypeDef GPIO_InitStructure; 
	  CAN_InitTypeDef        CAN_InitStructure;
  	CAN_FilterInitTypeDef  CAN_FilterInitStructure;
#if CAN2_RX1_INT_ENABLE 
   	NVIC_InitTypeDef  NVIC_InitStructure;
#endif
    //使能相关时钟
	  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);//使能PORTA时钟	                   											 

  	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN2, ENABLE);//使能CAN2时钟	

	  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_5|GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
    GPIO_Init(GPIOB, &GPIO_InitStructure);//初始化PA11,PA12
	
		GPIO_PinAFConfig(GPIOB,GPIO_PinSource5,GPIO_AF_CAN2); //GPIOA11复用为CAN2
	  GPIO_PinAFConfig(GPIOB,GPIO_PinSource6,GPIO_AF_CAN2); //GPIOA12复用为CAN2
    //初始化GPIO
	  //引脚复用映射配置

  	//CAN单元设置
   	CAN_InitStructure.CAN_TTCM=DISABLE;	//非时间触发通信模式   
  	CAN_InitStructure.CAN_ABOM=DISABLE;	//软件自动离线管理	  
  	CAN_InitStructure.CAN_AWUM=DISABLE;//睡眠模式通过软件唤醒(清除CAN->MCR的SLEEP位)
  	CAN_InitStructure.CAN_NART=ENABLE;	//禁止报文自动传送 
  	CAN_InitStructure.CAN_RFLM=DISABLE;	//报文不锁定,新的覆盖旧的  
  	CAN_InitStructure.CAN_TXFP=DISABLE;	//优先级由报文标识符决定 
  	CAN_InitStructure.CAN_Mode=mode;	 //模式设置 
  	CAN_InitStructure.CAN_SJW=tsjw;	//重新同步跳跃宽度(Tsjw)为tsjw+1个时间单位 CAN_SJW_1tq~CAN_SJW_4tq
  	CAN_InitStructure.CAN_BS1=tbs1; //Tbs1范围CAN_BS1_1tq ~CAN_BS1_16tq
  	CAN_InitStructure.CAN_BS2=tbs2;//Tbs2范围CAN_BS2_1tq ~	CAN_BS2_8tq
  	CAN_InitStructure.CAN_Prescaler=brp;  //分频系数(Fdiv)为brp+1	
  	CAN_Init(CAN2, &CAN_InitStructure);   // 初始化CAN2 
    
		//配置过滤器
 	  CAN_FilterInitStructure.CAN_FilterNumber=14;	  //过滤器14
  	CAN_FilterInitStructure.CAN_FilterMode=CAN_FilterMode_IdMask; 
  	CAN_FilterInitStructure.CAN_FilterScale=CAN_FilterScale_32bit; //32位 
  	CAN_FilterInitStructure.CAN_FilterIdHigh=0x0000;////32位ID
  	CAN_FilterInitStructure.CAN_FilterIdLow=0x0000;
  	CAN_FilterInitStructure.CAN_FilterMaskIdHigh=0x0000;//32位MASK
  	CAN_FilterInitStructure.CAN_FilterMaskIdLow=0x0000;
   	CAN_FilterInitStructure.CAN_FilterFIFOAssignment=CAN_Filter_FIFO1;//过滤器14关联到FIFO0
  	CAN_FilterInitStructure.CAN_FilterActivation=ENABLE; //激活过滤器14
  	CAN_FilterInit(&CAN_FilterInitStructure);//滤波器初始化
		//CAN_ITConfig(CAN2,CAN_IT_FMP0, ENABLE);//
		
#if CAN2_RX1_INT_ENABLE
	
	  CAN_ITConfig(CAN2,CAN_IT_FMP1,ENABLE);//FIFO0消息挂号中断允许.		    
  
  	NVIC_InitStructure.NVIC_IRQChannel = CAN2_RX1_IRQn;
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;     // 主优先级为1
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;            // 次优先级为0
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  	NVIC_Init(&NVIC_InitStructure);
#endif
	return 0;
}   
 
#if CAN2_RX1_INT_ENABLE	//使能RX0中断
//CAN接收中断
int i=0;
char byte0[12];
float this_yaw_angle;
float last_yaw_angle;
float temp_yaw_angle;
float D_last_yaw_angle, D_this_yaw_angle;
int32_t turn_cnt = 0;
 float Yaw_Angle_temp=0;
 float Yaw_Angle_Last=0;   //滤波用

float Gypo_Offset = 0.0;
float Yaw_Angle = 0.0;
	
//中断服务函数	
void CAN2_RX1_IRQHandler(void)
{
	if (CAN_GetITStatus(CAN2,CAN_IT_FMP1)!= RESET)
	{
		CAN_Receive(CAN2, 1 ,&RxMessage);
		//LED2=0;
		if(RxMessage.StdId==0X0401)
		 {
			 temp_yaw_angle = (int32_t)(RxMessage.Data[0]<<24)|(int32_t)(RxMessage.Data[1]<<16) 
							|(int32_t)(RxMessage.Data[2]<<8)|(int32_t)(RxMessage.Data[3]);
						
			 this_yaw_angle = ((float)temp_yaw_angle*0.01f);
			 
			 if(this_yaw_angle>36000||this_yaw_angle<-36000)
			 {
				 this_yaw_angle =  Yaw_Angle_temp;
			 }	
			 D_this_yaw_angle=(this_yaw_angle -Yaw_Angle_temp);			
			 if(D_this_yaw_angle > 500||D_this_yaw_angle<-500)
			 {
				 D_this_yaw_angle=D_last_yaw_angle;
			 }
							
			 D_last_yaw_angle=D_this_yaw_angle;
			 Yaw_Angle_temp  = Yaw_Angle_temp + D_this_yaw_angle;//将角度值转化为自然数域
			 Yaw_Angle=Yaw_Angle_temp-Gypo_Offset;				
		 } 
	 }
		 CAN_ClearITPendingBit(CAN2, CAN_IT_FMP1);
		 CAN_FIFORelease(CAN2,CAN_FIFO1); //清中断标志
}

#endif
//can发送一组数据(固定格式:ID为0X12,标准帧,数据帧)	
//ID :can报文中ID    
//msg:数据指针,最大为8个字节.
//返回值:0,成功;
//其他,失败;
u8 CAN2_Send_Msg(u8* msg,u8 ID)
{	
	
  u8 mbox;
  u16 i=0;
  CanTxMsg TxMessage;
  TxMessage.StdId=ID;	 // 标准标识符为0
  TxMessage.ExtId=ID;	 // 设置扩展标示符（29位）
  TxMessage.IDE=0;		  // 使用扩展标识符
  TxMessage.RTR=0;		  // 消息类型为数据帧，一帧8位
  TxMessage.DLC=8;							 // 发送两帧信息
  for(i=0;i<8;i++)
  TxMessage.Data[i]=msg[i];				 // 第一帧信息          
  mbox= CAN_Transmit(CAN2, &TxMessage);   
	//LED1=0;
  i=0;
  while((CAN_TransmitStatus(CAN2, mbox)==CAN_TxStatus_Failed)&&(i<0XFFF))i++;	//等待发送结束
  if(i>=0XFFF)
	{
		//LED2=0;
		return 1;
	}
  return 0;		
}

