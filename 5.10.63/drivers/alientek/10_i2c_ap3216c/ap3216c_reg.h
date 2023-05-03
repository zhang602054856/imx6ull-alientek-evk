#ifndef AP3216C_H
#define AP3216C_H


#define AP3216C_ADDR        0X1E    /* AP3216C i2c address */

/* AP3216C system register */
#define AP3216C_SYSTEM_CONG        0x00    /* System config    */
#define AP3216C_INT_STATUS        0X01    /* Interrupt status */
#define AP3216C_INT_CLEAR        0X02    /* INT clear manner */
#define AP3216C_IR_DATALOW        0x0A    /* IR data low         */
#define AP3216C_IR_DATAHIGH        0x0B    /* IR data high     */
#define AP3216C_ALS_DATALOW        0x0C    /* ALS data low     */
#define AP3216C_ALS_DATAHIGH    0X0D    /* ALS data high    */
#define AP3216C_PS_DATALOW        0X0E    /* PS data low      */
#define AP3216C_PS_DATAHIGH        0X0F    /* PS data high     */






#endif

