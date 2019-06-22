"""
    TRex events
"""
import datetime
import time

from .trex_types import listify
from .trex_exceptions import TRexError
from ..utils.text_opts import format_text


# an event
class Event(object):

    def __init__ (self, origin, ev_type, msg):
        self.origin   = origin
        self.ev_type  = ev_type
        self.msg      = msg

        self.ts = datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')

    def __str__ (self):

        prefix = "[{:^}][{:^}]".format(self.origin, self.ev_type)

        return "{:<10} - {:18} - {:}".format(self.ts, prefix, format_text(self.msg, 'bold'))



class EventsHandler(object):
    """
        Events Handler

        allows registering callbacks and trigger events
    """


    def __init__ (self):

        self.events = []

        # events are disabled by default until explicitly enabled
        self.enabled = False
        
        # no events handlers for now
        self.events_handlers = {}
        
        
    # will start handling events
    def enable (self):
        self.enabled = True
        

    def disable (self):
        self.enabled = False
        

    def is_enabled (self):
        return self.enabled
        
        
    def get_events (self, ev_type_filter = None):
        """
            returns a list of events

            'ev_type_filter' - 'info', 'warning' or a list of them
        """

        if ev_type_filter:
            return [ev for ev in self.events if ev.ev_type in listify(ev_type_filter)]
        else:
            return [ev for ev in self.events]


    def clear_events (self):
        """
            clears all the current events
        """
        self.events = []


    def register_event_handler (self, event_name, on_event_cb):
        """
            register an event handler

            associate 'event_name' with a callback

            when 'on_event' will be called with the event name, the callback 'on_event_cb'
            will be triggered

            'on_event_cb' should get *args, **kwargs and return None or EventLog

        """
        self.events_handlers[event_name] = on_event_cb


    def on_event (self, event_name, *args, **kwargs):
        """
            trigger an event

            if a handler is registered for 'event_name' it will be called
            and be passed with *args and **kwargs
        """
        if event_name not in self.events_handlers:
            raise TRexError("TRex Events: unhandled event '{0}'".format(event_name))


        event = self.events_handlers[event_name](*args, **kwargs)
        if event:
            self.events.append(event)
