#include "main.h"


int main()
{
	// Variáveis do programa principal
    int erro[2] = {0, 0}, n_linhas_ok = 0, servo = 0, esc = 0, x = 0, y = 0;
    int v_min = 1500000, v_max = 1800000, kp = 40000;


    // Seleciona a frequência da BBB para 1GHz
    system("cpufreq-set -g performance");

    // Objetos do programa principal
    Mat src;
    Capturador imagem(N_COLUNAS, N_LINHAS);
    Reconhecedor reconhecimento(N_COLUNAS, N_LINHAS, 5, 30, 50, 1);
    Controlador controle(N_COLUNAS, N_LINHAS, kp, 102400, 0, v_min, v_max);
    Pwm pwms;
    Leds leds;
    Buttons buttons;
    Trimpot trimpot0(0, 51200, 102400);     //trimpot kp
    Trimpot trimpot1(1, 1500000, 1800000);  //velocidade maxima
    Trimpot trimpot2(2, 1500000, 1800000);  //velocidade minima


	#ifdef TRATAMENTO_RAMPA
		// Declaração das variáveis necessárias para o tratamento da rampa e criação da thread auxiliar
		unsigned int ticks = 0;
		int tempo_rampa = 0;
		ticks_rampa = -INTERVALO_RAMPA;
		pthread_t tick_timer;
		pthread_create(&tick_timer, NULL, tickTimerThread, &ticks);
	#endif


	// Inicialização do robô -------------------------------------------------------------------------------------------
	#ifdef DEBUG
    	double ti = 0, tf = 0, frames = 0;
    	timeval tempo_inicio, tempo_fim;

		printf("Aguardando apertar o botao ESQUERDO para iniciar o robo...\n");
	#endif

    leds.setColor(RED);
    while(buttons.getStatus() != LEFT) usleep(250000);
	#ifdef DEBUG
		printf("2 segundos para o robo partir...\n");
	#endif
    pwms.initServo();
    pwms.initESC();
    v_min = trimpot2.getValue();
	controle.setVelocMin(v_min);
	usleep(50000);
	v_max = trimpot1.getValue();
	controle.setVelocMax(v_max);
	usleep(50000);
	kp = trimpot0.getValue();
	controle.setKp(kp);
	usleep(2000000);
    leds.setColor(GREEN);
    imagem.open();
	#ifdef TRATAMENTO_RAMPA
    	ticks = 0;
	#endif
	#ifdef DEBUG
		printf("Robo em funcionamento...\n");

		gettimeofday(&tempo_inicio, NULL);
	#endif


	// LOOP PRINCIPAL DO PROGRAMA ######################################################################################
    while(1)
    {
    	// Aquisição e Reconhecimento ----------------------------------------------------------------------------------
		imagem.getFrame(src);    // Captura um frame e retorna a imagem bin�ria

		erro[0] = erro[1];  // Guarda o erro anterior
		int estado = reconhecimento.algoritmo(src, &x, &y);

		if(estado >= 0)	// Realiza o algoritmo de reconhecimento
		{
			erro[1] = x - SP_X;	// Atualiza o valor do erro
			if(estado == 0)
			{
				leds.setColor(GREEN);
			}
		}
		else
		{
			leds.setColor(BLUE);
		}

		n_linhas_ok = N_LINHAS - y; // Atualiza o número de linhas válidas


		// Controle ----------------------------------------------------------------------------------------------------
		servo = controle.direcao(erro);         // Executa o controlador de direção
		esc = controle.velocidade(n_linhas_ok); // Executa o controlador de velocidade


		#ifdef TRAMENTO_RAMPA
			// Tratamento da rampa -------------------------------------------------------------------------------------
			if (rampa == SUBIDA)
			{
				esc += 100000;		// Habilita o turbo para a subida
			}
			else if (rampa == DESCIDA)
			{
				esc = ESC_NEUTRO;	// Habilita o freio para a descida
			}
			if (rampa > 0)
			{
				if (++tempo_rampa >= TEMPO_RAMPA)
				{
					rampa = 0;
					tempo_rampa = 0;
				}
			}
		#endif

		// Acionamento dos motores -------------------------------------------------------------------------------------
		pwms.updateServo(servo);  // Atualiza a posição do servo de acordo com o controle
		pwms.updateESC(esc);      // Atualiza a velocidade do motor de acordo com o controle


		// Partida ou Parada do Robô -----------------------------------------------------------------------------------
        if(buttons.getStatus() == RIGHT)
        {
        	// Parada do robô
        	pwms.updateESC(ESC_NEUTRO);
        	imagem.close();

        	// Reinicialização das variáveis
        	x = SP_X; y = 0; erro[0] = 0; erro[1] = 0;
        	reconhecimento.centro_frame_ant = SP_X;
			#ifdef TRAMENTO_RAMPA
        		ticks_rampa = -INTERVALO_RAMPA, rampa = 0;
			#endif

        	// Rotina de inicialização
        	leds.setColor(RED);
			#ifdef DEBUG
				printf("Aguardando apertar o botao ESQUERDO para iniciar o robo...\n");
			#endif
        	while(buttons.getStatus() != LEFT) usleep(250000);
        	pwms.updateServo(SERVO_NEUTRO);
			usleep(2000000);
			leds.setColor(GREEN);
			imagem.open();
			#ifdef TRAMENTO_RAMPA
				ticks = 0;
			#endif
			#ifdef DEBUG
				printf("Robo em funcionamento...\n");
			#endif
        }


		// Atualização das variáveis controladas pelos trimpots --------------------------------------------------------
        if(buttons.getStatus() == LEFT)
        {
        	v_min = trimpot2.getValue();
        	controle.setVelocMin(v_min);
        	usleep(10000);

        	v_max = trimpot1.getValue();
        	controle.setVelocMax(v_max);
        	usleep(10000);

        	kp = trimpot0.getValue();
        	controle.setKp(kp);
        	usleep(10000);
        }


		// Imprime na tela alguns par�metros durante o modo DEBUG (a cada 100 frames) ----------------------------------
		#ifdef DEBUG
        	frames++;
        	if(frames == 100)
        	{
        		gettimeofday(&tempo_fim, NULL);
        		tf = (double)tempo_fim.tv_usec + ((double)tempo_fim.tv_sec * (1000000.0));
				ti = (double)tempo_inicio.tv_usec + ((double)tempo_inicio.tv_sec * (1000000.0));
				printf("\n_-* FPS: %.3f *-_\n\n", frames / ((tf - ti) / 1000000.0));
        		frames = tf = ti = 0;
        		gettimeofday(&tempo_inicio, NULL);

				printf("Erro anterior: %d\n", erro[0]);
				printf("Erro atual: %d\n", erro[1]);
				printf("Numero de linhas OK: %d\n", n_linhas_ok);
				printf("Servo: %d\n", servo);
				printf("ESC (%d - %d): %d\n", v_min, v_max, esc);
				printf("Kp: %d\n", kp);

				#ifdef TRATAMENTO_RAMPA
					printf("Ticks: %d\n", ticks);
				#endif
			}
		#endif
    }

    imagem.close();

    return 0;
}


#ifdef TRATAMENTO_RAMPA
	// Thread auxiliar: 100 ms -> 10 Hz ********************************************************************************
	void* tickTimerThread(void *ticks_void_ptr)
	{
		unsigned int *ticks_ptr = (unsigned int *)ticks_void_ptr;

		int16_t gy;
		MPU6050 accelgyro;
		accelgyro.initialize();

		while(1)
		{
			(*ticks_ptr)++;

			// Realiza a leitura do eixo Y do giroscópio
			gy = accelgyro.getRotationY();

			// Verifica os eventos de subida ou descida, não permite eventos sucessivos em um intervalo de tempo
			if(gy < GY_SUBIDA && ((*ticks_ptr - ticks_rampa) >= INTERVALO_RAMPA))	// Identificação da subida
			{
				ticks_rampa = *ticks_ptr;
				rampa = SUBIDA;
			}
			else if(gy > GY_DESCIDA && ((*ticks_ptr - ticks_rampa) >= INTERVALO_RAMPA))	// Identificação da descida
			{
				ticks_rampa = *ticks_ptr;
				rampa = DESCIDA;
			}

			usleep(100000);
		}

		return NULL;
	}
#endif

