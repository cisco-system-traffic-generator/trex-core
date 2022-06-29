# Example of video streaming simulation

from trex.astf.api import *
import argparse


class Prof1():
    def __init__(self):
        pass

    def __setup_ip_gen(self, client_ip_range, server_ip_range):
        ip_gen_c = ASTFIPGenDist(ip_range=client_ip_range, distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=server_ip_range, distribution="seq")
        self.ip_gen = ASTFIPGen(dist_client=ip_gen_c, dist_server=ip_gen_s)

    def __setup_assoc(self, port, ip_start, ip_end, l7_offset):
        self.assoc = ASTFAssociationRule(port=port, ip_start=ip_start, ip_end=ip_end)
        self.l7_offset = l7_offset
        self.l7_assoc = ASTFAssociationRule(port=port, ip_start=ip_start, ip_end=ip_end, l7_map=l7_offset)

    def _setup_defaults(self):
        self.client_ip_range = ["16.0.0.0", "16.0.0.255"]

        self.port = 80
        self.server_ip_range = ["48.0.0.0", "48.0.255.255"]

        self.__setup_ip_gen(self.client_ip_range, self.server_ip_range)
        self.__setup_assoc(self.port, *self.server_ip_range, [0, 1, 2, 3])

    def _setup_params(self, speeds):
        self.template_names = {}
        for speed in speeds:
            self.template_names[speed] = '{:04}'.format(speed)

    def _create_client_program(self, speed_BPS, chunk_time, tg_data):
        prog_c = ASTFProgram(stream=True)

        prog_c.set_tick_var('base')

        prog_c.send(tg_data)
        prog_c.recv(int(speed_BPS * chunk_time))

        speeds = list(self.template_names)
        speeds.sort(reverse=True)
        for speed in speeds[:-1]:
            prog_c.jmp_dp('base', 'L{}:'.format(speed), speed_BPS/speed * chunk_time)

        prog_c.set_next_template(self.template_names[speeds[-1]])
        for speed in speeds[:-1]:
            prog_c.jmp('exit:')
            prog_c.set_label('L{}:'.format(speed))
            prog_c.set_next_template(self.template_names[speed])

        prog_c.set_label('exit:')

        return prog_c

    def _create_server_program(self, speed_BPS, chunk_time, tg_data):
        prog_s = ASTFProgram(stream=True)
        prog_s.recv(len(tg_data))
        prog_s.send('', int(speed_BPS * chunk_time))
        return prog_s

    def _create_template(self, speed_BPS, chunk_time, tg_name):
        prog_c = self._create_client_program(speed_BPS, chunk_time, tg_name)
        prog_s = self._create_server_program(speed_BPS, chunk_time, tg_name)

        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c, ip_gen=self.ip_gen, cps=0, port=self.port)
        temp_s = ASTFTCPServerTemplate(program=prog_s, assoc=self.l7_assoc)
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s, tg_name=tg_name)

        return template

    def _create_scheduler_program(self, video_time, chunk_time, buffer_time, initial_tg_name):
        prog = ASTFProgram(stream=False)
        prog.set_keepalive_msg(int((video_time+chunk_time)*1000)) # to prevent keepalive timeout

        # setup initial values
        prog.set_var("fetch_chunks", round(video_time/chunk_time))
        prog.set_var("saved_chunks", 0)
        play_time = chunk_time / 10  # playing resolution

        prog.set_next_template(initial_tg_name);

        prog.set_tick_var('base_time')
        prog.jmp('L_main_cond:')
        prog.set_label("L_main_loop:")
        if True:
            # fetch a chunk by the control of saved chunks level
            prog.jmp_gt('saved_chunks', 'L_play:', int(buffer_time/chunk_time))
            prog.jmp_le('fetch_chunks', 'L_play:', 0) # whole chunks are fetched
            if True:
                prog.set_tick_var('fetch_base')
                prog.exec_template()

                prog.add_var('saved_chunks', 1)
                prog.add_var('fetch_chunks', -1)
                prog.add_tick_stats('B', 'fetch_base')
                prog.jmp('L_fetch_end:')

            # simulate playing only
            prog.set_label('L_play:')
            if True:
                prog.delay(int(play_time * 1000000))
                prog.jmp('L_fetch_end:')

            prog.set_label('L_fetch_end:')

            # update played time
            prog.set_label('L_update_loop:')
            prog.jmp_dp('base_time', 'L_played_end:', chunk_time)
            if True:
                prog.jmp_le('saved_chunks', 'L_stall:', 0)
                if True:
                    prog.add_tick_var('base_time', chunk_time)
                    prog.add_var('saved_chunks', -1)
                    prog.jmp('L_update_loop:')

                prog.set_label('L_stall:')
                prog.jmp_le('fetch_chunks', 'L_played_end:', 0) # exit loop if no fetch
                if True:
                    prog.add_stats('A', 1) # stall count
                    prog.set_tick_var('base_time')
                    prog.jmp('L_played_end:')

            prog.set_label('L_played_end:')

        prog.set_label("L_main_cond:")
        prog.jmp_gt('fetch_chunks', 'L_main_loop:', 0)
        prog.jmp_gt('saved_chunks', 'L_main_loop:', 0)

        return prog

    def _create_initial_template(self, video_time, chunk_time, buffer_time, initial_speed):
        initial_tg_name = self.template_names[initial_speed]
        prog_c = self._create_scheduler_program(video_time, chunk_time, buffer_time, initial_tg_name)

        prog_s = ASTFProgram(stream=False)
        prog_s.close_msg()

        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c, ip_gen=self.ip_gen, port=self.port)
        temp_s = ASTFTCPServerTemplate(program=prog_s, assoc=self.assoc)
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        return template

    def create_profile(self, speeds, video_time, chunk_time, buffer_time):
        self._setup_defaults()
        self._setup_params(speeds)

        # initial active template
        speeds.sort(reverse=True)   # best quality first
        initial_speed = speeds[0]
        temp = self._create_initial_template(video_time, chunk_time, buffer_time, initial_speed)
        templates = [ temp ]

        # inactive templates
        for speed in speeds:
            temp = self._create_template(speed, chunk_time, self.template_names[speed])
            templates.append(temp)

        # profile
        profile = ASTFProfile(default_ip_gen=self.ip_gen, templates=templates)
        return profile

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--speeds',
                            type=list,
                            default=[558000, 264250, 115875, 75000, 43625],
                            help='')
        parser.add_argument('--video_time',
                            type=float,
                            default=60.0,
                            help='')
        parser.add_argument('--chunk_time',
                            type=float,
                            default=4.0,
                            help='')
        parser.add_argument('--buffer_time',
                            type=float,
                            default=10.0,
                            help='')

        args = parser.parse_args(tunables)

        return self.create_profile(args.speeds, args.video_time, args.chunk_time, args.buffer_time)


def register():
    return Prof1()

